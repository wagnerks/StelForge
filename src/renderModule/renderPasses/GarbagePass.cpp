#include "GarbagePass.h"

#include <algorithm>
#include <cmath>

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/Model.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/MaterialComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/GpuMaterialTable.h"
#include "renderModule/MaterialRules.h"
#include "renderModule/MaterialTextures.h"
#include "systemsModule/systems/GarbageSystem.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

namespace {
	// Metres past which no piece is drawn, and the size on screen, in pixels along its longest side, below which it is
	// left out, and at which each coarser level of detail takes over.
	constexpr float kMaxDistance = 250.f;
	constexpr float kMinPixels = 1.5f;
	constexpr float kLodPixels[] = { 90.f, 36.f, 14.f };
	// The cascades that draw pieces, a level of detail coarser than the camera sees them: past the first a piece is
	// hardly more than a texel, and its shadow is lost among the grass's.
	constexpr uint32_t kShadowCascades = 1;
}

void GarbagePass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(7, DescriptorType::StorageBuffer, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(20, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(21, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	DescriptorSetLayoutDesc textureDesc;
	for (uint32_t binding = SFE::DIFFUSE; binding <= SFE::TRIPLANAR2_NORMALS; ++binding) {
		textureDesc.bindings.emplace_back(binding, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	}
	mTextureLayout = rhi->createDescriptorSetLayout(textureDesc);

	GraphicsPipelineDesc base;
	auto& layout = base.layout;
	layout.bindings.push_back({ 0, sizeof(Vertex3D), VertexInputRate::PerVertex });
	layout.attributes.push_back({ 0, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, position)), true });
	layout.attributes.push_back({ 1, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, normal)), true });
	layout.attributes.push_back({ 2, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex3D, texCoords)), true });
	layout.attributes.push_back({ 3, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, tangent)), true });
	layout.attributes.push_back({ 4, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, biTangent)), true });
	layout.attributes.push_back({ 7, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex3D, texCoords1)), true });
	base.topology = PrimitiveTopology::Triangles;
	base.cullMode = CullMode::None;
	base.depthStencilState.depthTest = true;
	base.depthStencilState.depthWrite = true;
	base.depthStencilState.depthCompareOp = CompareOp::Less;
	base.pushConstantSize = sizeof(DrawParams);
	base.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
	base.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/garbage.vs");
	{
		RenderPassDesc passDesc;
		passDesc.colorAttachmentCount = 4;
		passDesc.colorAttachments[0] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA8 };
		passDesc.colorAttachments[1] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA16F };
		passDesc.colorAttachments[2] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA16F };
		passDesc.colorAttachments[3] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 0.f }, TextureFormat::RGBA16F };
		passDesc.hasDepthStencil = true;
		passDesc.depthAttachment = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, 1.f, 0, false, TextureFormat::DEPTH_COMPONENT24 };
		mGBufferPass = rhi->createRenderPass(passDesc);
		auto desc = base;
		desc.renderPass = mGBufferPass;
		desc.descriptorSetLayouts = { mLayout, mTextureLayout };
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, ComponentsModule::DefaultGBufferSolidFS);
		mSolidPipeline = rhi->createGraphicsPipeline(desc);
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, ComponentsModule::DefaultGBufferFS);
		mCutoutPipeline = rhi->createGraphicsPipeline(desc);
	}
	{
		RenderPassDesc passDesc;
		passDesc.hasDepthStencil = true;
		passDesc.depthAttachment.loadOp = AttachmentLoadOp::Load;
		passDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		passDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
		mShadowPass = rhi->createRenderPass(passDesc);
		auto desc = base;
		desc.renderPass = mShadowPass;
		desc.descriptorSetLayouts = { mLayout };
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/garbage_shadow.fs");
		mShadowPipeline = rhi->createGraphicsPipeline(desc);
	}
}

bool GarbagePass::setUp() {
	if (mTried) {
		return mEnabled;
	}
	auto* garbage = ECSHandler::getSystem<SystemsModule::GarbageSystem>();
	auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
	if (!garbage || !renderSystem) {
		return false;
	}
	mTried = true;
	size_t pieces = 0;
	garbage->readPieces([&](const auto& gpu, const auto&, auto&, const auto&) { pieces = gpu.size(); });
	if (pieces == 0 || !mSolidPipeline) {
		return false;
	}

	auto* rhi = Engine::instance()->rhi();
	auto* meshes = MeshVaoRegistry::instance();
	for (const auto& path : garbage->kindModels()) {
		auto& kind = mKinds.emplace_back();
		auto* model = AssetsModule::ModelLoader::instance()->load(path);
		if (!model || model->getLODs()->empty()) {
			continue;
		}
		for (auto* meshObject : model->getLODs()->front().meshes) {
			const auto& data = meshes->get(&meshObject->mesh);
			if (!data || data.indexCount == 0) {
				continue;
			}
			Submesh submesh;
			// The coarser levels are the mesh ids right after this one.
			const auto table = meshes->meshTable(data.meshId);
			for (uint32_t level = 0; level < (std::min)(data.lodCount, kLods) && level < table.size(); ++level) {
				auto lod = data;
				lod.firstIndex = table[level].firstIndex;
				lod.indexCount = table[level].indexCount;
				submesh.lods.push_back(lod);
			}
			const auto material = ComponentsModule::fromAssetMaterial(meshObject->material);
			submesh.material = renderSystem->materials().acquire(material);
			submesh.cutout = material.alphaDither || MaterialRules::cutoutCutoff(material.alphaCutoff, material.alphaFromMaps, material.fragmentShader) > 0.f;
			submesh.textures = rhi->allocateDescriptorSet(mTextureLayout);
			rhi->updateDescriptorSets(materialTextureWrites(submesh.textures, material.materials));
			kind.submeshes.push_back(std::move(submesh));
		}
	}

	std::vector<SystemsModule::GarbageSystem::GpuPiece> initial;
	garbage->readPieces([&](const auto& gpu, const auto&, auto& changed, const auto&) {
		initial = gpu;
		changed.clear();
	});
	BufferDesc piecesDesc;
	piecesDesc.size = initial.size() * sizeof(SystemsModule::GarbageSystem::GpuPiece);
	piecesDesc.usage = BufferUsage::Storage;
	piecesDesc.cpuAccess = CpuAccess::Write;
	piecesDesc.debugName = "Garbage pieces";
	mPieces = rhi->createBuffer(piecesDesc, initial.data());

	const uint32_t slots = (std::max)(1u, rhi->framesInFlight());
	mVisibleData.resize(initial.size());
	mVisible.resize(slots);
	mSets.resize(slots);
	for (uint32_t slot = 0; slot < slots; ++slot) {
		BufferDesc visibleDesc;
		visibleDesc.size = initial.size() * sizeof(uint32_t);
		visibleDesc.usage = BufferUsage::Storage;
		visibleDesc.cpuAccess = CpuAccess::Write;
		visibleDesc.debugName = "Garbage visible";
		mVisible[slot] = rhi->createBuffer(visibleDesc, mVisibleData.data());
		mSets[slot] = rhi->allocateDescriptorSet(mLayout);
	}
	mRuns.resize(mKinds.size() * kLods);
	mRunFirst.resize(mRuns.size());
	mRunCount.resize(mRuns.size());
	mEnabled = true;
	return true;
}

void GarbagePass::prepare(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK;
	mPrepared = false;
	if (!setUp()) {
		return;
	}
	auto* garbage = ECSHandler::getSystem<SystemsModule::GarbageSystem>();
	auto* rhi = Engine::instance()->rhi();
	// The kinds' materials may have just been added: up before the frame's first command list, not while it is drawn.
	auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
	const auto materials = renderSystem->materials().upload();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	const float pixelsPerMetre = static_cast<float>(screen.renderH) * 0.5f * renderDataHandle.current.projection[1][1];
	const auto& frustum = renderDataHandle.mCamFrustum;
	const auto eye = renderDataHandle.mCameraPos;
	const auto& kindOf = garbage->kindOf();
	const auto& sizes = garbage->kindSizes();
	const auto& cells = garbage->cells();

	for (auto& run : mRuns) {
		run.clear();
	}
	garbage->readPieces([&](const auto& pieces, const auto& hidden, auto& changed, const auto& bounds) {
		// What came to rest since last frame.
		for (const auto piece : changed) {
			rhi->updateBuffer(mPieces, piece * sizeof(SystemsModule::GarbageSystem::GpuPiece), sizeof(SystemsModule::GarbageSystem::GpuPiece), &pieces[piece]);
		}
		changed.clear();

		for (size_t c = 0; c < cells.size(); ++c) {
			const auto& cell = cells[c];
			if (cell.count == 0) {
				continue;
			}
			const auto& box = bounds[c];
			const Math::Vec3 lo{ box.lo[0] - 1.f, box.lo[1] - 1.f, box.lo[2] - 1.f };
			const Math::Vec3 hi{ box.hi[0] + 1.f, box.hi[1] + 1.f, box.hi[2] + 1.f };
			const Math::Vec3 nearest{ std::clamp(eye.x, lo.x, hi.x), std::clamp(eye.y, lo.y, hi.y), std::clamp(eye.z, lo.z, hi.z) };
			if (Math::length(nearest - eye) > kMaxDistance || !FrustumModule::AABB::isOnFrustum(frustum, (lo + hi) * 0.5f, (hi - lo) * 0.5f)) {
				continue;
			}
			for (uint32_t piece = cell.first; piece < cell.first + cell.count; ++piece) {
				if ((hidden[piece / 32] >> (piece % 32)) & 1u) {
					continue;
				}
				const auto& gpu = pieces[piece];
				const float dx = gpu.posScale[0] - eye.x;
				const float dy = gpu.posScale[1] - eye.y;
				const float dz = gpu.posScale[2] - eye.z;
				const float distance = (std::max)(std::sqrt(dx * dx + dy * dy + dz * dz), 0.1f);
				const float pixels = sizes[kindOf[piece]] * pixelsPerMetre / distance;
				if (pixels < kMinPixels) {
					continue;
				}
				uint32_t level = 0;
				while (level < std::size(kLodPixels) && pixels < kLodPixels[level]) {
					++level;
				}
				mRuns[kindOf[piece] * kLods + level].push_back(piece);
			}
		}
	});

	size_t used = 0;
	for (size_t r = 0; r < mRuns.size(); ++r) {
		mRunFirst[r] = static_cast<uint32_t>(used);
		mRunCount[r] = static_cast<uint32_t>(mRuns[r].size());
		std::copy(mRuns[r].begin(), mRuns[r].end(), mVisibleData.begin() + static_cast<std::ptrdiff_t>(used));
		used += mRuns[r].size();
	}
	if (used == 0) {
		return;
	}
	const size_t slot = rhi->frameInFlight() % mVisible.size();
	rhi->updateBuffer(mVisible[slot], 0, used * sizeof(uint32_t), mVisibleData.data());
	rhi->updateDescriptorSets({
		{ .set = mSets[slot], .binding = 5, .buffers{ { renderDataHandle.cameraMatricesUBO } } },
		{ .set = mSets[slot], .binding = 7, .buffers{ { materials } } },
		{ .set = mSets[slot], .binding = 20, .buffers{ { mPieces } } },
		{ .set = mSets[slot], .binding = 21, .buffers{ { mVisible[slot] } } },
	});
	mPrepared = true;
}

void GarbagePass::draw(const Render::CommandListHandle cmd, const bool shadow, const DrawParams& base) {
	auto* rhi = Engine::instance()->rhi();
	const auto set = mSets[rhi->frameInFlight() % mSets.size()];
	auto* meshes = MeshVaoRegistry::instance();
	PipelineHandle bound{};
	for (size_t k = 0; k < mKinds.size(); ++k) {
		const auto runs = mRunCount.begin() + static_cast<std::ptrdiff_t>(k * kLods);
		if (std::all_of(runs, runs + kLods, [](uint32_t count) { return count == 0; })) {
			continue;
		}
		for (const auto& submesh : mKinds[k].submeshes) {
			const auto pipeline = shadow ? mShadowPipeline : submesh.cutout ? mCutoutPipeline : mSolidPipeline;
			if (pipeline.id != bound.id) {
				bound = pipeline;
				rhi->cmdBindPipeline(cmd, pipeline);
				// After the pipeline: OpenGL keeps the vertex and index buffers on the pipeline's VAO.
				rhi->cmdBindVertexBuffer(cmd, 0, meshes->vertexBuffer(), 0);
				rhi->cmdBindIndexBuffer(cmd, meshes->indexBuffer(), IndexType::Uint32, 0);
			}
			if (shadow) {
				rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
			}
			else {
				const DescriptorSetHandle sets[] = { set, submesh.textures };
				rhi->cmdBindDescriptorSets(cmd, pipeline, 0, sets, 2);
			}
			auto params = base;
			params.params[1] = static_cast<float>(submesh.material);
			rhi->cmdPushConstants(cmd, pipeline, params);
			for (uint32_t level = 0; level < kLods; ++level) {
				const auto run = k * kLods + level;
				if (mRunCount[run] == 0 || submesh.lods.empty()) {
					continue;
				}
				const auto& lod = submesh.lods[(std::min)(static_cast<size_t>(level + (shadow ? 1 : 0)), submesh.lods.size() - 1)];
				rhi->cmdDrawIndexed(cmd, lod.indexCount, mRunCount[run], lod.firstIndex, static_cast<int32_t>(lod.firstVertex), mRunFirst[run]);
			}
		}
	}
}

void GarbagePass::drawShadow(SystemsModule::RenderData&, const Render::CommandListHandle cmd, const Math::Mat4& viewProj, const uint32_t layer) {
	if (!mPrepared || layer >= kShadowCascades) {
		return;
	}
	DrawParams params;
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			params.viewProj[c * 4 + r] = viewProj[c][r];
		}
	}
	params.params[0] = 1.f;
	draw(cmd, true, params);
}

void GarbagePass::render(SystemsModule::RenderData& renderDataHandle, const Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	if (!mPrepared) {
		return;
	}
	const auto gBuffer = renderDataHandle.frame.framebuffer(FrameId::GBufferFbo);
	if (!gBuffer) {
		return;
	}
	auto* rhi = Engine::instance()->rhi();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	RenderPassBeginInfo begin;
	begin.renderPass = mGBufferPass;
	begin.framebuffer = gBuffer;
	begin.width = screen.renderW;
	begin.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, begin);
	draw(cmd, false, DrawParams{});
	rhi->cmdEndRenderPass(cmd);
}
