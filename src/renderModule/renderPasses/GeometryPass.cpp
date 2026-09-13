#include "GeometryPass.h"

#include <algorithm>
#include "imgui.h"
#include "componentsModule/ModelComponent.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "renderModule/Utils.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "containersModule/Vector.h"
#include "core/ECSHandler.h"
#include "multithreading/ThreadPool.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "logsModule/logger.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;

namespace {
	struct GBufferMaterialPC {
		float roughness = 0.5f;
		float metallic = 0.f;
		float mapMask = 0.f;
		float alphaCutoff = 0.f;
		float uvSel = 0.f;
		float specFactor = 1.f;
		float clearcoat = 0.f;
		float clearcoatRough = 0.04f;
	};
}

void GeometryPass::fillBatcher(Batcher& batcher, const uint8_t nextRegistry, SystemsModule::RenderData& renderData) {
	batcher.clear();

	ecss::Ranges<ecss::EntityId> entities;
	{
		const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
		if (octreeSys) {
			entities = octreeSys->collectVisibleRanges(renderData.mNextCamFrustum);
		}
	}

	auto addMeshes = [&](auto&& viewObjs) -> size_t {
		size_t considered = 0;
		size_t visible = 0;
		for (auto [ent, transform, meshComp, matComp] : viewObjs) {
			if (!meshComp || !transform) {
				continue;
			}
			++considered;
			size_t added = 0;
			meshComp->forEachDrawMesh(matComp, [&](const SFE::RenderMeshData& mesh, const MaterialComponent* mat) {
				if (mat && mat->transparent) {
					return;
				}
				batcher.addToDrawList(mesh, mat, ent);
				++added;
			});
			if (added > 0) {
				++visible;
			}
		}
		auto* info = SystemsModule::RenderDebugInfo::instance();
		info->cullConsidered = considered;
		info->cullVisible = visible;
		return visible;
	};

	auto& drawReg = ECSHandler::drawRegistry(nextRegistry);
	size_t visible = 0;
	if (!entities.empty()) {
		visible = addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>(entities));
	}
	else {
		addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
	}
}

void GeometryPass::prepare() {
	auto curPassData = getContainer().getCurrentPassData();
	auto outlineData = mOutlineData.getCurrentPassData();
		curPassData->markPreparing();

	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, outlineData, renderData]() mutable {
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
		outlineData->getBatcher().clear();
		fillBatcher(curPassData->getBatcher(), nextRegistry, renderData);

		ecss::Ranges<ecss::EntityId> entities;
		{
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			if (octreeSys) {
				entities = octreeSys->collectVisibleRanges(renderData.mNextCamFrustum);
			}
		}

		{
			auto& outlineBatcher = outlineData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcherOutline)
			for (const auto& [entity, outline, transform, meshComp, armatComp] : ECSHandler::drawRegistry(nextRegistry).view<const OutlineComponent, const ComponentsModule::TransformMatComp, const MeshComponent, const ComponentsModule::ArmatureBonesComponent>()) {
				if (!entities.empty() && !entities.contains(entity)) {
					continue;
				}

				if (!meshComp) {
					continue;
				}

				/*for (const auto& mesh : meshComp->meshGraph) {
					outlineBatcher.addToDrawList(mesh.value.vaoId, mesh.value.verticesCount, mesh.value.indicesCount, {}, transform->mTransform, armatComp ? const_cast<Math::Mat4*>(armatComp->boneMatrices.data()) : nullptr);
				}*/

				for (const auto& mesh : meshComp->meshGraph) {
					outlineBatcher.addToDrawList(mesh.value, nullptr, entity);
				}
			}

			//outlineBatcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());
		}
	});
}

void GeometryPass::init(SystemsModule::RenderData& renderDataHandle) {
	if (mInited) {
		return;
	}
	mInited = true;

	mOutlineData.init(2);
	getContainer().init(2);

	const auto w = Engine::instance()->getWindow()->getScreenData().renderW;
	const auto h = Engine::instance()->getWindow()->getScreenData().renderH;

	auto rhi = Engine::instance()->rhi();
	mGpuFrames = (std::max)(1u, (std::min)(2u, rhi->framesInFlight()));
	auto* renderSystem = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>();
	const auto frameLayout = renderDataHandle.frameSetLayout
		? renderDataHandle.frameSetLayout
		: (renderSystem ? renderSystem->frameSetLayoutHandle() : Render::DescriptorSetLayoutHandle{});

	SamplerDesc positionSamplerDesc;
	positionSamplerDesc.minFilter = FilterMode::Nearest;
	positionSamplerDesc.magFilter = FilterMode::Nearest;
	mSampler = rhi->createSampler(positionSamplerDesc);

	TextureDesc positionBufDesc;
	positionBufDesc.width = w;
	positionBufDesc.height = h;
	positionBufDesc.format = TextureFormat::RGBA32F;
	positionBufDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;

	TextureDesc normalBufDesc;
	normalBufDesc.width = w;
	normalBufDesc.height = h;
	normalBufDesc.format = TextureFormat::RGBA16F;
	normalBufDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;

	TextureDesc albedoBufDesc;
	albedoBufDesc.width = w;
	albedoBufDesc.height = h;
	albedoBufDesc.format = TextureFormat::RGBA16F;
	albedoBufDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;

	TextureDesc depthTex{};
	depthTex.width = w;
	depthTex.height = h;
	depthTex.depth = 1;
	depthTex.dimension = TextureDimension::Tex2D;
	depthTex.format = TextureFormat::DEPTH_COMPONENT24;
	depthTex.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;

	RenderPassDesc geometryPassDesc;
	geometryPassDesc.colorAttachmentCount = 3;
	geometryPassDesc.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f}, TextureFormat::RGBA32F };
	geometryPassDesc.colorAttachments[1] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f}, TextureFormat::RGBA16F };
	geometryPassDesc.colorAttachments[2] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f}, TextureFormat::RGBA16F };
	geometryPassDesc.hasDepthStencil = true;
	geometryPassDesc.depthAttachment = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, 1.f, 0, false, TextureFormat::DEPTH_COMPONENT24 };
	geometryPass = rhi->createRenderPass(geometryPassDesc);

	for (uint32_t i = 0; i < mGpuFrames; ++i) {
		auto& slot = mFrames[i];
		slot.positionBuffer = rhi->createTexture(positionBufDesc);
		slot.normalBuffer = rhi->createTexture(normalBufDesc);
		slot.albedoBuffer = rhi->createTexture(albedoBufDesc);
		slot.depthsBuffer = rhi->createTexture(depthTex);
		FramebufferDesc mainFboDesc;
		mainFboDesc.width = w;
		mainFboDesc.height = h;
		mainFboDesc.renderPass = geometryPass;
		mainFboDesc.colorAttachments = { {slot.positionBuffer}, {slot.normalBuffer}, {slot.albedoBuffer} };
		mainFboDesc.depthStencilAttachment = FramebufferAttachmentDesc{.texture = slot.depthsBuffer };
		slot.gFramebuffer = rhi->createFramebuffer(mainFboDesc);
	}

	TextureDesc outlinesBufDesc;
	outlinesBufDesc.width = w;
	outlinesBufDesc.height = h;
	outlinesBufDesc.format = TextureFormat::RGBA8;
	outlinesBufDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
	outlinesMask = rhi->createTexture(outlinesBufDesc);
	outlinesBuffer = rhi->createTexture(outlinesBufDesc);

	TextureDesc lightsBufDesc;
	lightsBufDesc.width = w;
	lightsBufDesc.height = h;
	lightsBufDesc.format = TextureFormat::RGBA8;
	lightsBufDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
	lightsBuffer = rhi->createTexture(lightsBufDesc);

	GraphicsPipelineDesc pipelineDesc;

	Render::VertexLayoutDesc& layout = pipelineDesc.layout;
	layout.bindings.push_back({ 0,sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	layout.attributes.push_back({ 0,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	layout.attributes.push_back({ 1,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)),  true });
	layout.attributes.push_back({ 2,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	layout.attributes.push_back({ 3,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	layout.attributes.push_back({ 4,0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	layout.attributes.push_back({ 5,0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false }); //todo only for dynamic mesh
	layout.attributes.push_back({ 6,0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });
	layout.attributes.push_back({ 7,0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords1)), true });
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.fillMode = PolygonFillMode::Fill;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	pipelineDesc.blend.enabled = false;
	pipelineDesc.renderPass = geometryPass;
	if (frameLayout) {
		pipelineDesc.descriptorSetLayouts = { frameLayout };
	}
	pipelineDesc.shaders.vertex = Engine::instance()->rhi()->loadShader(ShaderStage::Vertex, "shaders/g_buffer.vs");
	pipelineDesc.shaders.fragment = Engine::instance()->rhi()->loadShader(ShaderStage::Fragment, "shaders/g_buffer.fs");
	pipelineDesc.pushConstantSize = static_cast<uint32_t>(sizeof(GBufferMaterialPC));
	pipelineDesc.pushConstantStages = ShaderStage::Fragment;

	Render::DescriptorSetLayoutDesc texLayoutDescGBuf;
	texLayoutDescGBuf.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(5, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(6, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	gBuffetLayout = rhi->createDescriptorSetLayout(texLayoutDescGBuf);
	gBuffetLayoutSet = rhi->allocateDescriptorSet(gBuffetLayout);
	getContainer().setMaterialLayout(gBuffetLayout);
	if (frameLayout) {
		pipelineDesc.descriptorSetLayouts.push_back(gBuffetLayout);
	}

	geometryPipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);
	mGBufferPipelineDesc = pipelineDesc;
	mGBufferPipelines.emplace(std::string(ComponentsModule::DefaultGBufferVS) + "|" + ComponentsModule::DefaultGBufferFS, geometryPipeline);

	RenderPassDesc gbrp;
	gbrp.colorAttachmentCount = 1;
	gbrp.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 0.f}, TextureFormat::RGBA8 };
	outlineBufferRenderPass = rhi->createRenderPass(gbrp);

	RenderPassDesc outlineRPD;
	outlineRPD.colorAttachmentCount = 1;
	outlineRPD.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 0.f}, TextureFormat::RGBA8 };
	outlineRenderPass = rhi->createRenderPass(outlineRPD);

	FramebufferDesc outlineFboDesc;
	outlineFboDesc.width = w;
	outlineFboDesc.height = h;
	outlineFboDesc.renderPass = outlineBufferRenderPass;
	outlineFboDesc.colorAttachments = { {outlinesMask} };
	outlineFramebuffer = rhi->createFramebuffer(outlineFboDesc);

	FramebufferDesc outlineResultDesc;
	outlineResultDesc.width = w;
	outlineResultDesc.height = h;
	outlineResultDesc.renderPass = outlineRenderPass;
	outlineResultDesc.colorAttachments = { {outlinesBuffer} };
	outlineResultFbo = rhi->createFramebuffer(outlineResultDesc);

	GraphicsPipelineDesc gBufferOutlinePDesc;
	gBufferOutlinePDesc.layout = layout;
	gBufferOutlinePDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/g_buffer_outlines.vs");
	gBufferOutlinePDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/g_buffer_outlines.fs");
	gBufferOutlinePDesc.topology = PrimitiveTopology::Triangles;
	gBufferOutlinePDesc.cullMode = CullMode::Back;
	gBufferOutlinePDesc.frontFace = FrontFace::CounterClockWise;
	gBufferOutlinePDesc.depthStencilState.depthTest = false;
	gBufferOutlinePDesc.depthStencilState.depthWrite = false;
	gBufferOutlinePDesc.renderPass = outlineBufferRenderPass;
	if (frameLayout) {
		gBufferOutlinePDesc.descriptorSetLayouts = { frameLayout };
		mOutlineFrameSet = rhi->allocateDescriptorSet(frameLayout);
	}
	gBufferOutlinePipeline = rhi->createGraphicsPipeline(gBufferOutlinePDesc);

	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(25, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(26, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(27, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	mTexSetLayoutOutlines = rhi->createDescriptorSetLayout(texLayoutDesc);
	mTexSetOutlines = rhi->allocateDescriptorSet(mTexSetLayoutOutlines);

	GraphicsPipelineDesc outlinePDesc;
	outlinePDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/g_outline.vs");
	outlinePDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/g_outline.fs");
	outlinePDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	outlinePDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	outlinePDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	outlinePDesc.topology = PrimitiveTopology::Triangles;
	outlinePDesc.cullMode = CullMode::None;
	outlinePDesc.depthStencilState.depthTest = false;
	outlinePDesc.depthStencilState.depthWrite = false;
	outlinePDesc.renderPass = outlineRenderPass;
	outlinePDesc.descriptorSetLayouts = { mTexSetLayoutOutlines };
	outlinePipeline = rhi->createGraphicsPipeline(outlinePDesc);

	initGrid(frameLayout);

	publish(renderDataHandle);
}

void GeometryPass::publish(SystemsModule::RenderData& renderDataHandle) {
	auto& frame = renderDataHandle.frame;
	const auto s = mSampler;
	const auto& slot = mFrames[Engine::instance()->rhi()->frameInFlight() % mGpuFrames];
	frame.set(FrameId::GBufferFbo, { .framebuffer = slot.gFramebuffer });
	frame.set(FrameId::GBufferOutlineFbo, { .framebuffer = outlineFramebuffer });
	frame.set(FrameId::GBufferPosition, { .texture = slot.positionBuffer, .sampler = s });
	frame.set(FrameId::GBufferNormal, { .texture = slot.normalBuffer, .sampler = s });
	frame.set(FrameId::GBufferAlbedo, { .texture = slot.albedoBuffer, .sampler = s });
	frame.set(FrameId::GBufferOutlines, { .texture = outlinesBuffer, .sampler = s });
	frame.set(FrameId::GBufferLights, { .texture = lightsBuffer, .sampler = s });
	frame.set(FrameId::GBufferDepth, { .texture = slot.depthsBuffer, .sampler = s });
}

void GeometryPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}

	publish(renderDataHandle);
	auto rhi = Engine::instance()->rhi();
	const auto& slot = mFrames[rhi->frameInFlight() % mGpuFrames];
	const bool wireframe = renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME;

	getContainer().getCurrentPassData()->waitReady();
	auto* curPassData = getContainer().getCurrentPassData();
	auto* outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();

	RenderPassBeginInfo geometryPassBegin;
	geometryPassBegin.renderPass = geometryPass;
	geometryPassBegin.framebuffer = slot.gFramebuffer;
	geometryPassBegin.width = Engine::instance()->getWindow()->getScreenData().renderW;
	geometryPassBegin.height = Engine::instance()->getWindow()->getScreenData().renderH;

	rhi->cmdBeginRenderPass(cmd, geometryPassBegin);
	const auto fillMode = wireframe ? PolygonFillMode::Line : PolygonFillMode::Fill;
	if (!curPassData->getBatcher().drawList.empty()) {
		curPassData->getBatcher().flushAll(cmd, gBuffetLayoutSet, [this, fillMode](const std::string& vs, const std::string& fs) {
			return getGBufferPipeline(vs, fs, fillMode);
		}, [this, fillMode](Render::CommandListHandle drawCmd, const DrawObject& obj) {
			const auto pipe = getGBufferPipeline(obj.vertexShader, obj.fragmentShader, fillMode);
			GBufferMaterialPC pc;
			pc.roughness = obj.roughness;
			pc.metallic = obj.metallic;
			pc.mapMask = obj.gBufferMapMask();
			pc.uvSel = obj.gBufferUvSel();
			pc.specFactor = obj.specFactor;
			pc.clearcoat = obj.clearcoat;
			pc.clearcoatRough = obj.clearcoatRough;
			if (obj.alphaDither) {
				pc.alphaCutoff = 1.78f;
			} else {
				pc.alphaCutoff = obj.cutoutCutoff();
			}
			Engine::instance()->rhi()->cmdPushConstants(drawCmd, pipe, ShaderStage::Fragment, &pc, sizeof(pc));
		});
	}
	// The grid keeps its own pipeline, so it stays filled in wireframe mode -- which is what it
	// already did, since binding a pipeline reset the polygon mode on OpenGL anyway.
	drawGrid(renderDataHandle, cmd);
	rhi->cmdEndRenderPass(cmd);

	if (outlinePipeline && outlineFramebuffer && !outlineData->getBatcher().drawList.empty()) {
		const int rw = Engine::instance()->getWindow()->getScreenData().renderW;
		const int rh = Engine::instance()->getWindow()->getScreenData().renderH;
		needClearOutlines = true;

		RenderPassBeginInfo beginGBuf;
		beginGBuf.renderPass = outlineBufferRenderPass;
		beginGBuf.framebuffer = outlineFramebuffer;
		beginGBuf.width = rw;
		beginGBuf.height = rh;
		rhi->cmdBeginRenderPass(cmd, beginGBuf);
		rhi->cmdBindPipeline(cmd, gBufferOutlinePipeline);
		if (mOutlineFrameSet && renderDataHandle.transformsSSBO) {
			rhi->updateDescriptorSets({
				{
					.set = mOutlineFrameSet,
					.binding = 1,
					
					.buffers{ {renderDataHandle.transformsSSBO} }
				},
				{
					.set = mOutlineFrameSet,
					.binding = 2,
					
					.buffers{ {renderDataHandle.bonesSSBO} }
				},
				{
					.set = mOutlineFrameSet,
					.binding = 5,
					
					.buffers{ {renderDataHandle.cameraMatricesUBO} }
				}
			});
		}
		outlineData->getBatcher().flushAll(cmd, mOutlineFrameSet, {}, {}, nullptr, false);
		rhi->cmdEndRenderPass(cmd);

		if (outlineResultFbo) {
			RenderPassBeginInfo beginOutlines;
			beginOutlines.renderPass = outlineRenderPass;
			beginOutlines.framebuffer = outlineResultFbo;
			beginOutlines.width = rw;
			beginOutlines.height = rh;
			rhi->cmdBeginRenderPass(cmd, beginOutlines);
			rhi->cmdBindPipeline(cmd, outlinePipeline);

			std::vector<Render::WriteDescriptor> writes;
			auto bindIfChanged = [&](uint32_t binding, TextureHandle tex, TextureHandle& prev) {
				if (tex.id == prev.id) {
					return;
				}
				prev = tex;
				writes.push_back({
					.set = mTexSetOutlines,
					.binding = binding,
					
					.images{ {tex, mSampler} }
				});
			};
			bindIfChanged(25, lightsBuffer, mBoundOutlineLights);
			bindIfChanged(26, slot.normalBuffer, mBoundOutlineNormal);
			bindIfChanged(27, outlinesMask, mBoundOutlineMask);
			if (!writes.empty()) {
				rhi->updateDescriptorSets(writes);
			}
			rhi->cmdBindDescriptorSets(cmd, outlinePipeline, 0, &mTexSetOutlines, 1);
			Utils::renderQuad(cmd);
			rhi->cmdEndRenderPass(cmd);
		}
	}
	else if (outlinePipeline && needClearOutlines) {
		const int rw = Engine::instance()->getWindow()->getScreenData().renderW;
		const int rh = Engine::instance()->getWindow()->getScreenData().renderH;
		if (outlineFramebuffer) {
			RenderPassBeginInfo beginGBuf;
			beginGBuf.renderPass = outlineBufferRenderPass;
			beginGBuf.framebuffer = outlineFramebuffer;
			beginGBuf.width = rw;
			beginGBuf.height = rh;
			rhi->cmdBeginRenderPass(cmd, beginGBuf);
			rhi->cmdEndRenderPass(cmd);
		}
		if (outlineResultFbo) {
			RenderPassBeginInfo beginOutlines;
			beginOutlines.renderPass = outlineRenderPass;
			beginOutlines.framebuffer = outlineResultFbo;
			beginOutlines.width = rw;
			beginOutlines.height = rh;
			rhi->cmdBeginRenderPass(cmd, beginOutlines);
			rhi->cmdEndRenderPass(cmd);
		}
		needClearOutlines = false;
	}
}

SFE::Render::PipelineHandle GeometryPass::getGBufferPipeline(const std::string& vertexShader, const std::string& fragmentShader,
	const Render::PolygonFillMode fillMode) {
	const std::string vs = vertexShader.empty() ? ComponentsModule::DefaultGBufferVS : vertexShader;
	const std::string fs = fragmentShader.empty() ? ComponentsModule::DefaultGBufferFS : fragmentShader;
	const auto key = vs + "|" + fs + (fillMode == PolygonFillMode::Line ? "|line" : "|fill");
	if (const auto it = mGBufferPipelines.find(key); it != mGBufferPipelines.end()) {
		return it->second;
	}

	auto* rhi = Engine::instance()->rhi();
	const auto vert = rhi->loadShader(ShaderStage::Vertex, vs);
	const auto frag = rhi->loadShader(ShaderStage::Fragment, fs);
	if (!vert || !frag) {
		LogsModule::Logger::LOG_WARNING("GBuffer shader missing: %s / %s", vs.c_str(), fs.c_str());
		return geometryPipeline;
	}

	auto desc = mGBufferPipelineDesc;
	desc.shaders.vertex = vert;
	desc.shaders.fragment = frag;
	desc.fillMode = fillMode;
	const auto pipeline = rhi->createGraphicsPipeline(desc);
	mGBufferPipelines.emplace(key, pipeline);
	return pipeline;
}

void GeometryPass::initGrid(const Render::DescriptorSetLayoutHandle frameLayout) {
	auto* rhi = Engine::instance()->rhi();
	std::vector<SFE::Vertex3D> vertices = {
		{{ 1.f, 0.f, -1.f}, {1.f,1.f}, {0.f,1.f,0.f}},
		{{ 1.f, 0.f,  1.f}, {1.f,0.f}, {0.f,1.f,0.f}},
		{{-1.f, 0.f,  1.f}, {0.f,0.f}, {0.f,1.f,0.f}},
		{{-1.f, 0.f, -1.f}, {0.f,1.f}, {0.f,1.f,0.f}},
	};
	std::vector<unsigned> indices = { 0, 2, 1, 3, 2, 0 };

	BufferDesc vboDesc;
	vboDesc.size = vertices.size() * sizeof(SFE::Vertex3D);
	vboDesc.usage = BufferUsage::Vertex;
	vboDesc.cpuAccess = CpuAccess::None;
	mGridVBO = rhi->createBuffer(vboDesc, vertices.data());

	BufferDesc eboDesc;
	eboDesc.size = indices.size() * sizeof(unsigned);
	eboDesc.usage = BufferUsage::Index;
	eboDesc.cpuAccess = CpuAccess::None;
	mGridEBO = rhi->createBuffer(eboDesc, indices.data());

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.push_back({ 0, sizeof(SFE::Vertex3D), VertexInputRate::PerVertex });
	pipelineDesc.layout.attributes.push_back({ 0, 0, VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), false });
	pipelineDesc.layout.attributes.push_back({ 1, 0, VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, normal)), true });
	pipelineDesc.layout.attributes.push_back({ 2, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex3D, texCoords)), true });
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/grid.vs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/grid.fs");
	pipelineDesc.renderPass = geometryPass;
	if (frameLayout) {
		pipelineDesc.descriptorSetLayouts = { frameLayout };
	}
	pipelineDesc.pushConstantSize = static_cast<uint32_t>(sizeof(GridDrawData));
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.fillMode = PolygonFillMode::Fill;
	pipelineDesc.depthStencilState = { true, true, CompareOp::LessEqual };
	pipelineDesc.blend = { false };
	mGridPipeline = rhi->createGraphicsPipeline(pipelineDesc);
}

void GeometryPass::drawGrid(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mGridPipeline) {
		return;
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(camera);
	auto* rhi = Engine::instance()->rhi();
	rhi->cmdBindPipeline(cmd, mGridPipeline);
	if (auto* renderSystem = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()) {
		const auto frameSet = renderSystem->frameSetHandle();
		if (frameSet) {
			rhi->cmdBindDescriptorSets(cmd, mGridPipeline, 0, &frameSet, 1);
		}
	}
	rhi->cmdBindIndexBuffer(cmd, mGridEBO, IndexType::Uint32);
	rhi->cmdBindVertexBuffer(cmd, 0, mGridVBO);
	mGridDrawData.viewPos.xyz = renderDataHandle.mCameraPos;
	mGridDrawData.far = cameraComp->getProjection().getFar();
	mGridDrawData.near = cameraComp->getProjection().getNear();
	rhi->cmdPushConstants(cmd, mGridPipeline, mGridDrawData);
	rhi->cmdDrawIndexed(cmd, 6);
}
