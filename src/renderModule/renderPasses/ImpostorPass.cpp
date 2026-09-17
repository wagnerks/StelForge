#include "ImpostorPass.h"

#include <algorithm>
#include <cmath>

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/Model.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/VegetationComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/MaterialRules.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/VegetationSystem.h"

using namespace SFE::Render::RenderPasses;

namespace {
	// Match sfe_impostor.glsl.
	constexpr int kViews = 8;
	constexpr int kLevels = 5;
	// A level 0 frame's side in texels. Big enough for a tree at the distance its entity gives way.
	constexpr uint32_t kFrameSize = 192;
	// How far a light's picture of a tree is pushed away from the light, in half sizes.
	constexpr float kShadowPush = 0.5f;
}

void ImpostorPass::init(SystemsModule::RenderData&) {
	auto& registry = ECSHandler::registry();
	ComponentsModule::VegetationComponent vegetation;
	for (const auto& [id, vegetationComp, placements] : registry.view<const ComponentsModule::VegetationComponent, const ComponentsModule::VegetationPlacementsComponent>()) {
		if (!vegetationComp || !placements) {
			continue;
		}
		vegetation = *vegetationComp;
		// Every model any impostor layer uses, once.
		std::vector<std::vector<uint32_t>> modelOf(vegetation.layers.size());
		for (size_t l = 0; l < vegetation.layers.size(); ++l) {
			if (!vegetation.layers[l].impostor || vegetation.layers[l].pitch != 0.f) {
				continue;
			}
			for (const auto& path : vegetation.layers[l].models) {
				const auto it = std::find(mModels.begin(), mModels.end(), path);
				modelOf[l].push_back(static_cast<uint32_t>(it - mModels.begin()));
				if (it == mModels.end()) {
					mModels.push_back(path);
				}
			}
		}
		if (mModels.empty()) {
			return;
		}

		// The frame each model is baked into: centred on its vertical axis, since it turns around that, and wide and
		// tall enough for all of it.
		for (const auto& path : mModels) {
			float radius = 0.f;
			float lo = 0.f;
			float hi = 0.f;
			bool any = false;
			if (auto* model = AssetsModule::ModelLoader::instance()->load(path); model && !model->getLODs()->empty()) {
				for (const auto* mesh : model->getLODs()->front().meshes) {
					for (const auto& v : mesh->mesh.vertices) {
						radius = (std::max)(radius, std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z));
						lo = any ? (std::min)(lo, v.position.y) : v.position.y;
						hi = any ? (std::max)(hi, v.position.y) : v.position.y;
						any = true;
					}
				}
			}
			const float half = (std::max)({ radius, (hi - lo) * 0.5f, 0.01f });
			mModelFrames.insert(mModelFrames.end(), { half, (lo + hi) * 0.5f, 0.f, 0.f });
		}

		// The instances in the placements' own cell order, so a cell's are one range here too.
		mCellSize = placements->cellSize;
		mCellsPerSide = placements->cellsPerSide;
		mOrigin = placements->origin;
		mPlacements = placements->instances.size();
		const size_t cellCount = static_cast<size_t>(mCellsPerSide) * static_cast<size_t>(mCellsPerSide);
		mCells.resize(cellCount);
		std::vector<GpuInstance> instances;
		for (size_t c = 0; c < cellCount; ++c) {
			auto& cell = mCells[c];
			cell.first = static_cast<uint32_t>(instances.size());
			for (uint32_t i = placements->cellStart[c]; i < placements->cellStart[c + 1]; ++i) {
				const auto& placement = placements->instances[i];
				const auto& layer = vegetation.layers[placement.layer];
				if (!layer.impostor || layer.pitch != 0.f) {
					continue;
				}
				const uint32_t model = modelOf[placement.layer][placement.model];
				const float half = mModelFrames[model * 4] * placement.scale;
				const float top = placement.pos.y + (mModelFrames[model * 4 + 1] * placement.scale + half);
				cell.minY = cell.count == 0 ? placement.pos.y : (std::min)(cell.minY, placement.pos.y);
				cell.maxY = cell.count == 0 ? top : (std::max)(cell.maxY, top);
				mMaxHalfSize = (std::max)(mMaxHalfSize, half);
				++cell.count;

				GpuInstance instance;
				instance.posScale[0] = placement.pos.x;
				instance.posScale[1] = placement.pos.y;
				instance.posScale[2] = placement.pos.z;
				instance.posScale[3] = placement.scale;
				instance.yawModel[0] = placement.yaw * 0.0174532925f;
				instance.yawModel[1] = static_cast<float>(model);
				instance.yawModel[2] = static_cast<float>(i);
				instances.push_back(instance);
			}
		}
		if (instances.empty()) {
			return;
		}

		auto* rhi = Engine::instance()->rhi();
		BufferDesc instanceDesc;
		instanceDesc.size = instances.size() * sizeof(GpuInstance);
		instanceDesc.usage = BufferUsage::Storage;
		instanceDesc.cpuAccess = CpuAccess::None;
		instanceDesc.debugName = "Impostor instances";
		mInstances = rhi->createBuffer(instanceDesc, instances.data());
		BufferDesc modelsDesc = instanceDesc;
		modelsDesc.size = mModelFrames.size() * sizeof(float);
		modelsDesc.debugName = "Impostor models";
		mModelsBuffer = rhi->createBuffer(modelsDesc, mModelFrames.data());
		break;
	}
	if (!mInstances) {
		return;
	}
	mEnabled = true;

	auto* rhi = Engine::instance()->rhi();
	const uint32_t slots = (std::max)(1u, rhi->framesInFlight());
	mBitsData.assign((mPlacements + 31) / 32 + 1, 0u);
	mBits.resize(slots);
	mBitsUploaded.assign(slots, UINT64_MAX);
	mSets.resize(slots);
	for (auto& bits : mBits) {
		BufferDesc desc;
		desc.size = mBitsData.size() * sizeof(uint32_t);
		desc.usage = BufferUsage::Storage;
		desc.cpuAccess = CpuAccess::Write;
		desc.debugName = "Impostor entity bits";
		bits = rhi->createBuffer(desc, mBitsData.data());
	}

	const std::array<Vertex2D, 4> quad{ {
		{ { -1.f, -1.f }, { 0.f, 0.f } },
		{ { 1.f, -1.f }, { 1.f, 0.f } },
		{ { -1.f, 1.f }, { 0.f, 1.f } },
		{ { 1.f, 1.f }, { 1.f, 1.f } },
	} };
	BufferDesc quadDesc;
	quadDesc.size = sizeof(quad);
	quadDesc.usage = BufferUsage::Vertex;
	quadDesc.cpuAccess = CpuAccess::None;
	mQuad = rhi->createBuffer(quadDesc, quad.data());

	SamplerDesc sampler;
	sampler.addressU = AddressMode::ClampToEdge;
	sampler.addressV = AddressMode::ClampToEdge;
	mSampler = rhi->createSampler(sampler);

	// The atlas: level 0 on the left, the coarser levels stacked in the column to its right.
	mAtlasWidth = kFrameSize * kViews + kFrameSize / 2 * kViews;
	mAtlasHeight = kFrameSize * static_cast<uint32_t>(mModels.size());
	TextureDesc atlasDesc;
	atlasDesc.width = mAtlasWidth;
	atlasDesc.height = mAtlasHeight;
	atlasDesc.format = TextureFormat::RGBA8;
	atlasDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
	mAlbedo = rhi->createTexture(atlasDesc);
	mNormal = rhi->createTexture(atlasDesc);
	TextureDesc depthDesc = atlasDesc;
	depthDesc.format = TextureFormat::DEPTH_COMPONENT24;
	depthDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;
	mBakeDepth = rhi->createTexture(depthDesc);

	// Baking.
	{
		RenderPassDesc passDesc;
		passDesc.colorAttachmentCount = 2;
		passDesc.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 0.f }, TextureFormat::RGBA8 };
		passDesc.colorAttachments[1] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, { 0.5f, 1.f, 0.5f, 0.f }, TextureFormat::RGBA8 };
		passDesc.hasDepthStencil = true;
		passDesc.depthAttachment = { AttachmentLoadOp::Clear, AttachmentStoreOp::DontCare, 1.f, 0, false, TextureFormat::DEPTH_COMPONENT24 };
		mBakePass = rhi->createRenderPass(passDesc);
		FramebufferDesc fb;
		fb.width = mAtlasWidth;
		fb.height = mAtlasHeight;
		fb.renderPass = mBakePass;
		fb.colorAttachments = { { mAlbedo }, { mNormal } };
		fb.depthStencilAttachment = FramebufferAttachmentDesc{ .texture = mBakeDepth };
		mBakeFbo = rhi->createFramebuffer(fb);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.emplace_back(1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		mBakeLayout = rhi->createDescriptorSetLayout(layoutDesc);

		GraphicsPipelineDesc desc;
		desc.layout.bindings.push_back({ 0, sizeof(Vertex3D), VertexInputRate::PerVertex });
		desc.layout.attributes.push_back({ 0, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, position)), true });
		desc.layout.attributes.push_back({ 1, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex3D, normal)), true });
		desc.layout.attributes.push_back({ 2, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex3D, texCoords)), true });
		desc.topology = PrimitiveTopology::Triangles;
		desc.cullMode = CullMode::None;
		desc.depthStencilState.depthTest = true;
		desc.depthStencilState.depthWrite = true;
		desc.depthStencilState.depthCompareOp = CompareOp::Less;
		desc.renderPass = mBakePass;
		desc.descriptorSetLayouts = { mBakeLayout };
		desc.pushConstantSize = sizeof(BakeParams);
		desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/impostor_bake.vs");
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/impostor_bake.fs");
		mBakePipeline = rhi->createGraphicsPipeline(desc);
	}

	// Drawing.
	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(20, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(21, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(22, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(23, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(24, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);

	GraphicsPipelineDesc base;
	base.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	base.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, position)), false);
	base.topology = PrimitiveTopology::TriangleStrip;
	base.cullMode = CullMode::None;
	base.depthStencilState.depthTest = true;
	base.depthStencilState.depthWrite = true;
	base.depthStencilState.depthCompareOp = CompareOp::Less;
	base.descriptorSetLayouts = { mLayout };
	base.pushConstantSize = sizeof(DrawParams);
	base.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
	base.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/impostor.vs");
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
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/impostor.fs");
		mPipeline = rhi->createGraphicsPipeline(desc);
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
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/impostor_shadow.fs");
		mShadowPipeline = rhi->createGraphicsPipeline(desc);
	}
}

void ImpostorPass::bakeIfNeeded(const Render::CommandListHandle cmd) {
	if (mEnabled && !mBaked) {
		bake(cmd);
		mBaked = true;
	}
}

void ImpostorPass::releaseBakeTarget() {
	// A few frames on, once no command list still recording or in flight names it. Counted here, once a frame:
	// bakeIfNeeded runs from the shadow pass as well, and counting there let the framebuffer go while the frame that
	// baked with it was still on the GPU.
	if (mBakeDepth && mBaked && ++mFramesSinceBake > Engine::instance()->rhi()->framesInFlight() + 1) {
		auto* rhi = Engine::instance()->rhi();
		rhi->destroyFramebuffer(mBakeFbo);
		rhi->destroyTexture(mBakeDepth);
		mBakeFbo = {};
		mBakeDepth = {};
	}
}

void ImpostorPass::bake(const Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	auto* rhi = Engine::instance()->rhi();
	auto* registry = MeshVaoRegistry::instance();

	// Every submesh up in the shared buffers before they are bound: an upload may replace them.
	struct Submesh {
		RenderMeshData mesh;
		TextureHandle texture;
		float tint[4]{};
	};
	std::vector<std::vector<Submesh>> submeshes(mModels.size());
	for (size_t m = 0; m < mModels.size(); ++m) {
		auto* model = AssetsModule::ModelLoader::instance()->load(mModels[m]);
		if (!model || model->getLODs()->empty()) {
			continue;
		}
		for (auto* meshObject : model->getLODs()->front().meshes) {
			const auto& data = registry->get(&meshObject->mesh);
			if (!data || data.indexCount == 0) {
				continue;
			}
			const auto material = ComponentsModule::fromAssetMaterial(meshObject->material);
			float cutoff = MaterialRules::cutoutCutoff(material.alphaCutoff, material.alphaFromMaps, material.fragmentShader);
			// Above 1 a dithered coverage; a picture only needs where the leaves are.
			cutoff = cutoff > 1.f ? 0.5f : cutoff;
			Submesh submesh{ data, MaterialRules::diffuseTexture(material.materials), { material.tint.x, material.tint.y, material.tint.z, cutoff } };
			submeshes[m].push_back(submesh);
		}
	}

	RenderPassBeginInfo begin;
	begin.renderPass = mBakePass;
	begin.framebuffer = mBakeFbo;
	begin.width = mAtlasWidth;
	begin.height = mAtlasHeight;
	rhi->cmdBeginRenderPass(cmd, begin);
	rhi->cmdBindPipeline(cmd, mBakePipeline);
	// After the pipeline: OpenGL keeps the vertex and index buffers on the pipeline's VAO.
	rhi->cmdBindVertexBuffer(cmd, 0, registry->vertexBuffer(), 0);
	rhi->cmdBindIndexBuffer(cmd, registry->indexBuffer(), IndexType::Uint32, 0);
	const float width = static_cast<float>(mAtlasWidth);
	const float height = static_cast<float>(mAtlasHeight);
	for (size_t m = 0; m < mModels.size(); ++m) {
		for (int level = 0; level < kLevels; ++level) {
			const float size = static_cast<float>(kFrameSize >> level);
			float cornerX = level == 0 ? 0.f : static_cast<float>(kFrameSize * kViews);
			float cornerY = 0.f;
			for (int l = 1; l < level; ++l) {
				cornerY += static_cast<float>(mModels.size()) * static_cast<float>(kFrameSize >> l);
			}
			for (int view = 0; view < kViews; ++view) {
				const float angle = static_cast<float>(view) / static_cast<float>(kViews) * 6.2831853f;
				BakeParams params;
				params.view[0] = std::sin(angle);
				params.view[1] = std::cos(angle);
				params.view[2] = mModelFrames[m * 4];
				params.view[3] = mModelFrames[m * 4 + 1];
				params.frame[0] = (cornerX + static_cast<float>(view) * size) / width;
				params.frame[1] = (cornerY + static_cast<float>(m) * size) / height;
				params.frame[2] = size / width;
				params.frame[3] = size / height;
				for (const auto& submesh : submeshes[m]) {
					std::copy(std::begin(submesh.tint), std::end(submesh.tint), params.tint);
					rhi->cmdPushConstants(cmd, mBakePipeline, params);
					const auto set = mBakeTextures.get(mBakeLayout, 1, submesh.texture);
					rhi->cmdBindDescriptorSets(cmd, mBakePipeline, 0, &set, 1);
					rhi->cmdDrawIndexed(cmd, submesh.mesh.indexCount, 1, submesh.mesh.firstIndex, static_cast<int32_t>(submesh.mesh.firstVertex));
				}
			}
		}
	}
	rhi->cmdEndRenderPass(cmd);
}

SFE::Render::DescriptorSetHandle ImpostorPass::setFor(const BufferHandle cameraUbo) {
	auto* rhi = Engine::instance()->rhi();
	const size_t slot = rhi->frameInFlight() % mSets.size();

	if (auto* vegetation = ECSHandler::getSystem<SystemsModule::VegetationSystem>()) {
		vegetation->copyEntityBits(mBitsData, mBitsVersion);
	}
	if (mBitsUploaded[slot] != mBitsVersion && !mBitsData.empty()) {
		rhi->updateBuffer(mBits[slot], 0, (std::min)(mBitsData.size(), (mPlacements + 31) / 32 + 1) * sizeof(uint32_t), mBitsData.data());
		mBitsUploaded[slot] = mBitsVersion;
	}

	auto& set = mSets[slot][cameraUbo.id];
	if (!set) {
		set = rhi->allocateDescriptorSet(mLayout);
		rhi->updateDescriptorSets({
			{ .set = set, .binding = 5, .buffers{ { cameraUbo } } },
			{ .set = set, .binding = 20, .buffers{ { mInstances } } },
			{ .set = set, .binding = 21, .buffers{ { mBits[slot] } } },
			{ .set = set, .binding = 22, .buffers{ { mModelsBuffer } } },
			{ .set = set, .binding = 23, .images{ { mAlbedo, mSampler } } },
			{ .set = set, .binding = 24, .images{ { mNormal, mSampler } } },
		});
	}
	return set;
}

void ImpostorPass::drawCells(const Render::CommandListHandle cmd, const PipelineHandle pipeline, const DrawParams& params, const BufferHandle cameraUbo,
	const FrustumModule::Frustum& frustum) {
	auto* rhi = Engine::instance()->rhi();
	const auto set = setFor(cameraUbo);
	rhi->cmdBindPipeline(cmd, pipeline);
	// After the pipeline: OpenGL keeps the vertex buffer on the pipeline's VAO.
	rhi->cmdBindVertexBuffer(cmd, 0, mQuad, 0);
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1);
	rhi->cmdPushConstants(cmd, pipeline, params);

	// A row's visible cells next to each other are one draw; empty cells do not break it.
	for (int cz = 0; cz < mCellsPerSide; ++cz) {
		uint32_t first = 0;
		uint32_t end = 0;
		const auto flush = [&] {
			if (end > first) {
				rhi->cmdDraw(cmd, 4, end - first, 0, first);
			}
			first = end = 0;
		};
		for (int cx = 0; cx < mCellsPerSide; ++cx) {
			const auto& cell = mCells[static_cast<size_t>(cz) * mCellsPerSide + cx];
			if (cell.count == 0) {
				continue;
			}
			const Math::Vec3 centre{ mOrigin.x + (cx + 0.5f) * mCellSize, (cell.minY + cell.maxY) * 0.5f, mOrigin.y + (cz + 0.5f) * mCellSize };
			const Math::Vec3 extents{ mCellSize * 0.5f + mMaxHalfSize, (cell.maxY - cell.minY) * 0.5f, mCellSize * 0.5f + mMaxHalfSize };
			if (!FrustumModule::AABB::isOnFrustum(frustum, centre, extents)) {
				flush();
				continue;
			}
			if (end == first) {
				first = cell.first;
			}
			end = cell.first + cell.count;
		}
		flush();
	}
}

void ImpostorPass::drawGBuffer(const Render::CommandListHandle cmd, const BufferHandle cameraUbo, const FrustumModule::Frustum& frustum, const Math::Vec3& eye,
	const float texelsPerMetre) {
	if (!mEnabled || !mBaked || !mPipeline) {
		return;
	}
	DrawParams params;
	params.eye[0] = eye.x;
	params.eye[1] = eye.y;
	params.eye[2] = eye.z;
	params.atlas[0] = static_cast<float>(kFrameSize);
	params.atlas[1] = static_cast<float>(mAtlasWidth);
	params.atlas[2] = static_cast<float>(mAtlasHeight);
	params.atlas[3] = static_cast<float>(mModels.size());
	params.params[1] = texelsPerMetre;
	drawCells(cmd, mPipeline, params, cameraUbo, frustum);
}

void ImpostorPass::drawShadow(const Render::CommandListHandle cmd, const BufferHandle cameraUbo, const Math::Mat4& viewProj, const FrustumModule::Frustum& frustum,
	const Math::Vec3& direction, const float texelsPerMetre) {
	if (!mEnabled || !mBaked || !mShadowPipeline) {
		return;
	}
	DrawParams params;
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			params.viewProj[c * 4 + r] = viewProj[c][r];
		}
	}
	params.eye[0] = direction.x;
	params.eye[1] = direction.y;
	params.eye[2] = direction.z;
	params.eye[3] = 1.f;
	params.atlas[0] = static_cast<float>(kFrameSize);
	params.atlas[1] = static_cast<float>(mAtlasWidth);
	params.atlas[2] = static_cast<float>(mAtlasHeight);
	params.atlas[3] = static_cast<float>(mModels.size());
	params.params[0] = kShadowPush;
	params.params[1] = texelsPerMetre;
	drawCells(cmd, mShadowPipeline, params, cameraUbo, frustum);
}

void ImpostorPass::render(SystemsModule::RenderData& renderDataHandle, const Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	if (!mEnabled) {
		return;
	}
	const auto gBuffer = renderDataHandle.frame.framebuffer(FrameId::GBufferFbo);
	if (!gBuffer) {
		return;
	}
	bakeIfNeeded(cmd);
	releaseBakeTarget();

	auto* rhi = Engine::instance()->rhi();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	RenderPassBeginInfo begin;
	begin.renderPass = mGBufferPass;
	begin.framebuffer = gBuffer;
	begin.width = screen.renderW;
	begin.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, begin);
	drawGBuffer(cmd, renderDataHandle.cameraMatricesUBO, renderDataHandle.mCamFrustum, renderDataHandle.mCameraPos,
		static_cast<float>(screen.renderH) * 0.5f * renderDataHandle.current.projection[1][1]);
	rhi->cmdEndRenderPass(cmd);
}
