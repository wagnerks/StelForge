#include "GlassPass.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/Material.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "mathModule/VectorOperations.h"
#include "mathModule/MatrixOperators.h"
#include "multithreading/ThreadPool.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ChunksSystem.h"

using namespace SFE::Render::RenderPasses;

void GlassPass::prepare() {
	auto* renderSys = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>();
	if (!renderSys) {
		return;
	}
	auto curPassData = getContainer().getCurrentPassData();
	if (!curPassData) {
		return;
	}
	curPassData->markPreparing();

	auto& renderData = renderSys->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, curPassData, camFrustum = renderData.mNextCamFrustum]() mutable {
		FUNCTION_BENCHMARK;
		struct ReadyGuard {
			RenderPassData* data = nullptr;
			~ReadyGuard() { if (data) { data->markReady(); } }
		} ready{ curPassData };
		auto* renderSys = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>();
		auto worldLock = renderSys ? renderSys->tryLockWorldRead() : SFE::SystemsModule::RenderSystem::WorldReadGuard{};
		if (renderSys && !worldLock) {
			return;
		}
		curPassData->getBatcher().clear();

		ecss::Ranges<ecss::EntityId> entities;
		{
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			if (octreeSys) {
				entities = octreeSys->collectVisibleRanges(camFrustum);
			}
		}

		auto& batcher = curPassData->getBatcher();
		auto& drawReg = ECSHandler::drawRegistry(nextRegistry);
		auto addMeshes = [&](auto&& viewObjs) {
			for (auto [ent, transform, meshComp, matComp] : viewObjs) {
				if (!meshComp || !transform) {
					continue;
				}
				meshComp->forEachDrawMesh(matComp, [&](const SFE::RenderMeshData& mesh, const MaterialComponent* mat) {
					if (!mat || !mat->transparent) {
						return;
					}
					batcher.addToDrawListGlass(mesh, *mat, ent);
				});
			}
		};
		const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
		if (entities.empty() || !octreeSys || octreeSys->empty()) {
			addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
		}
		else {
			addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>(entities));
			if (batcher.drawList.empty()) {
				addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
			}
		}
	});
}

void GlassPass::init(SystemsModule::RenderData& renderDataHandle) {
	getContainer().init(2);

	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	TextureDesc copyDesc;
	copyDesc.width = w;
	copyDesc.height = h;
	copyDesc.format = TextureFormat::RGBA16F;
	copyDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mSceneCopy = rhi->createTexture(copyDesc);

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	// Off-screen refraction must not ClampToEdge — that stretches the last
	// column/row into a visible frame. Mirror keeps real scene content.
	samplerDesc.addressU = AddressMode::MirrorRepeat;
	samplerDesc.addressV = AddressMode::MirrorRepeat;
	samplerDesc.addressW = AddressMode::MirrorRepeat;
	mSampler = rhi->createSampler(samplerDesc);

	FramebufferDesc copyFb;
	copyFb.width = w;
	copyFb.height = h;
	copyFb.colorAttachments = { { mSceneCopy } };
	mSceneCopyFbo = rhi->createFramebuffer(copyFb);

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
	rpDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	rpDesc.colorAttachments[0].format = TextureFormat::RGBA16F;
	rpDesc.hasDepthStencil = true;
	rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Load;
	rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
	rpDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT24;
	mRenderPass = rhi->createRenderPass(rpDesc);

	BufferDesc uboDesc;
	uboDesc.usage = BufferUsage::Uniform;
	uboDesc.cpuAccess = CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resolutionX = static_cast<float>(w);
	mParams.resolutionY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(6, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(13, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(4, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(2, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(3, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, DescriptorType::UniformBuffer, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mLayout);

	GraphicsPipelineDesc pipelineDesc;
	Render::VertexLayoutDesc& layout = pipelineDesc.layout;
	layout.bindings.push_back({ 0, sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	layout.attributes.push_back({ 0, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	layout.attributes.push_back({ 1, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)), true });
	layout.attributes.push_back({ 2, 0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	layout.attributes.push_back({ 3, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	layout.attributes.push_back({ 4, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	layout.attributes.push_back({ 5, 0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false });
	layout.attributes.push_back({ 6, 0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::LessEqual;
	pipelineDesc.blend.enabled = true;
	pipelineDesc.blend.srcColor = BlendFactor::One;
	pipelineDesc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
	pipelineDesc.renderPass = mRenderPass;
	pipelineDesc.descriptorSetLayouts = { mLayout };
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/g_buffer.vs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/glass.fs");
	mPipeline = rhi->createGraphicsPipeline(pipelineDesc);
}

void GlassPass::ensureTargets(SystemsModule::RenderData& renderDataHandle) {
	auto& frame = renderDataHandle.frame;
	const auto finalTex = frame.texture(FrameId::Final);
	const auto depthTex = frame.texture(FrameId::GBufferDepth);
	if (!finalTex || !depthTex) {
		return;
	}
	if (mGlassFbo && mFinalTex.id == finalTex.id && mDepthTex.id == depthTex.id) {
		return;
	}

	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	FramebufferDesc glassFb;
	glassFb.width = w;
	glassFb.height = h;
	glassFb.renderPass = mRenderPass;
	glassFb.colorAttachments = { { finalTex } };
	glassFb.depthStencilAttachment = { .texture = depthTex };
	mGlassFbo = rhi->createFramebuffer(glassFb);
	mFinalTex = finalTex;
	mDepthTex = depthTex;
}

void GlassPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	const auto curPassData = getContainer().getCurrentPassData();
	curPassData->waitReady();
	getContainer().rotate();
	prepare();

	if (curPassData->getBatcher().drawList.empty()) {
		return;
	}

	ensureTargets(renderDataHandle);
	const auto srcFbo = renderDataHandle.frame.framebuffer(FrameId::Final);
	if (!mGlassFbo || !srcFbo || !mPipeline) {
		return;
	}

	auto& frame = renderDataHandle.frame;

	mParams.viewPosX = renderDataHandle.mCameraPos.x;
	mParams.viewPosY = renderDataHandle.mCameraPos.y;
	mParams.viewPosZ = renderDataHandle.mCameraPos.z;
	mParams.roughness = renderDataHandle.graphics.glassRoughness;
	mParams.tintX = 1.f;
	mParams.tintY = 1.f;
	mParams.tintZ = 1.f;
	mParams.sphereSpacing = 8.f;
	if (auto* chunks = ECSHandler::getSystem<SystemsModule::ChunksSystem>()) {
		if (const auto* dd = static_cast<const SystemsModule::ChunksDebugData*>(chunks->getDebugData())) {
			mParams.sphereSpacing = dd->sphereSpacing;
		}
	}
	if (const auto* csm = frame.data<CsmFrame>(FrameId::Csm)) {
		mParams.lightX = csm->lightDirection.x;
		mParams.lightY = csm->lightDirection.y;
		mParams.lightZ = csm->lightDirection.z;
		mParams.colorX = csm->lightColor.x;
		mParams.colorY = csm->lightColor.y;
		mParams.colorZ = csm->lightColor.z;
	}

	std::vector<WriteDescriptor> writes;
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 6;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ mSceneCopy, mSampler });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 15;
		w.type = DescriptorType::UniformBuffer;
		w.buffers.push_back({ mParamsUBO });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblIrradiance)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 12;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::IblIrradiance), frame.sampler(FrameId::IblIrradiance) });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblPrefilter)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 13;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::IblPrefilter), frame.sampler(FrameId::IblPrefilter) });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.cameraMatricesUBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 5;
		w.type = DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.transformsSSBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 1;
		w.type = DescriptorType::StorageBuffer;
		w.buffers.push_back({ renderDataHandle.transformsSSBO });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.bonesSSBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 2;
		w.type = DescriptorType::StorageBuffer;
		w.buffers.push_back({ renderDataHandle.bonesSSBO });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblBrdf)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 14;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::IblBrdf), frame.sampler(FrameId::IblBrdf) });
		writes.push_back(std::move(w));
	}
	rhi->updateDescriptorSets(writes);

	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;
	RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mGlassFbo;
	beginInfo.renderPass = mRenderPass;
	beginInfo.width = w;
	beginInfo.height = h;

	const Math::Mat4& view = renderDataHandle.current.view;

	auto entityPos = [&](ecss::EntityId id) -> Math::Vec3 {
		if (auto tm = ECSHandler::registry().pinComponent<const ComponentsModule::TransformMatComp>(id)) {
			return Math::Vec3(tm->mTransform[3]);
		}
		if (auto tc = ECSHandler::registry().pinComponent<const TransformComponent>(id)) {
			return tc->getPos(true);
		}
		return {};
	};

	auto viewDepthKey = [&](ecss::EntityId id) -> int32_t {
		const Math::Vec3 p = entityPos(id);
		const Math::Vec4 viewP = view * Math::Vec4(p.x, p.y, p.z, 1.f);
		return static_cast<int32_t>(std::floor(-viewP.z * 64.f));
	};

	struct BatchDrawItem {
		DrawObject* batch = nullptr;
		int32_t depthKey = 0;
	};

	std::vector<BatchDrawItem> batches;
	batches.reserve(curPassData->getBatcher().drawList.size());
	for (auto& [_, obj] : curPassData->getBatcher().drawList) {
		if (obj.entities.empty()) {
			continue;
		}
		int32_t maxDepth = std::numeric_limits<int32_t>::min();
		for (ecss::EntityId id : obj.entities) {
			maxDepth = std::max(maxDepth, viewDepthKey(id));
		}
		batches.push_back({ &obj, maxDepth });
	}

	std::stable_sort(batches.begin(), batches.end(), [](const BatchDrawItem& a, const BatchDrawItem& b) {
		return a.depthKey > b.depthKey;
	});

	std::vector<DrawObject*> ordered;
	ordered.reserve(batches.size());
	for (const BatchDrawItem& item : batches) {
		ordered.push_back(item.batch);
	}

	auto beforeDraw = [this, rhi](Render::CommandListHandle drawCmd, const DrawObject& o) {
		mParams.ior = o.ior > 1.f ? o.ior : 1.52f;
		mParams.opacity = o.opacity > 0.f ? o.opacity : 0.22f;
		mParams.tintX = o.tint.x;
		mParams.tintY = o.tint.y;
		mParams.tintZ = o.tint.z;
		rhi->cmdUpdateBuffer(drawCmd, mParamsUBO, mParams);

		auto* white = AssetsModule::TextureHandler::loadTexture("white.png");
		if (!white) {
			return;
		}
		Render::TextureHandle diff = white->texture;
		for (uint8_t i = 0; i < o.materialData.materialsCount; ++i) {
			const auto& mat = o.materialData.material[i];
			if (mat.slot == SFE::DIFFUSE && mat.texture) {
				diff = mat.texture;
				break;
			}
		}
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 4;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ diff, white->sampler });
		rhi->updateDescriptorSets({ std::move(w) });
	};

	auto getPipeline = [this](const std::string&, const std::string&) {
		return mPipeline;
	};

	rhi->cmdBlit(cmd, srcFbo, mSceneCopyFbo, BlitMask::Color, BlitFilter::Nearest);
	rhi->cmdBeginRenderPass(cmd, beginInfo);

	curPassData->getBatcher().flushAll(cmd, mSet, getPipeline, beforeDraw, &ordered, false);

	rhi->cmdEndRenderPass(cmd);
}
