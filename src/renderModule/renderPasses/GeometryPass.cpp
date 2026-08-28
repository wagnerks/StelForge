#include "GeometryPass.h"

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
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/TransformComponent.h"
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

void GeometryPass::prepare() {
	auto curPassData = getContainer().getCurrentPassData();
	auto outlineData = mOutlineData.getCurrentPassData();
		curPassData->markPreparing();

	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camFrustum = renderData.mNextCamFrustum, outlineData]() mutable {
		FUNCTION_BENCHMARK;
		curPassData->getBatcher().clear();
		outlineData->getBatcher().clear();

		ecss::Ranges<ecss::EntityId> entities;
		{
			FUNCTION_BENCHMARK_NAMED(GeometryPass_octree);
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			entities = octreeSys->collectVisibleRanges(camFrustum);
		}

		{
			FUNCTION_BENCHMARK_NAMED(addedToBatcher);
			auto& batcher = curPassData->getBatcher();
			auto& drawReg = ECSHandler::drawRegistry(nextRegistry);
			auto addMeshes = [&](auto&& viewObjs) {
				size_t considered = 0;
				size_t visible = 0;
				for (auto [ent, transform, meshComp, matComp] : viewObjs) {
					if (!meshComp || !transform || (matComp && matComp->transparent)) {
						continue;
					}
					++considered;
					++visible;
					for (const auto& mesh : meshComp->meshGraph) {
						batcher.addToDrawList(mesh.value, matComp, ent);
					}
				}
				auto* info = SystemsModule::RenderDebugInfo::instance();
				info->cullConsidered = considered;
				info->cullVisible = visible;
			};
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			if (entities.empty() && octreeSys->empty()) {
				addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>());
			}
			else {
				addMeshes(drawReg.view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent>(entities));
			}
		}

		{
			auto& outlineBatcher = outlineData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcherOutline)
			for (const auto& [entity, outline, transform, meshComp, armatComp] : ECSHandler::drawRegistry(nextRegistry).view<const OutlineComponent, const ComponentsModule::TransformMatComp, const MeshComponent, const ComponentsModule::ArmatureBonesComponent>()) {
				if (!entities.contains(entity)) {//need to check - if draw all outline objects is faster then cull all of them such way?//todo
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
		curPassData->markReady();
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
	TextureDesc positionBufDesc;
	positionBufDesc.width = w;
	positionBufDesc.height = h;
	positionBufDesc.format = TextureFormat::RGBA32F;

	SamplerDesc positionSamplerDesc;
	positionSamplerDesc.minFilter = FilterMode::Nearest;
	positionSamplerDesc.magFilter = FilterMode::Nearest;
	mData.sampler = rhi->createSampler(positionSamplerDesc);

	// position color buffer
	mData.positionBuffer = rhi->createTexture(positionBufDesc);

	// normal color buffer
	TextureDesc normalBufDesc;
	normalBufDesc.width = w;
	normalBufDesc.height = h;
	normalBufDesc.format = TextureFormat::RGBA16F;
	mData.normalBuffer = rhi->createTexture(normalBufDesc);

	// color + specular color buffer
	TextureDesc albedoBufDesc;
	albedoBufDesc.width = w;
	albedoBufDesc.height = h;
	albedoBufDesc.format = TextureFormat::RGBA16F;
	mData.albedoBuffer = rhi->createTexture(albedoBufDesc);

	// outline buffer
	TextureDesc outlinesBufDesc;
	outlinesBufDesc.width = w;
	outlinesBufDesc.height = h;
	outlinesBufDesc.format = TextureFormat::RGBA8;
	mData.outlinesBuffer = rhi->createTexture(outlinesBufDesc);

	// light buffer
	TextureDesc lightsBufDesc;
	lightsBufDesc.width = w;
	lightsBufDesc.height = h;
	lightsBufDesc.format = TextureFormat::RGBA8;
	mData.lightsBuffer = rhi->createTexture(lightsBufDesc);

	FramebufferDesc mainFboDesc;
	mainFboDesc.width = w;
	mainFboDesc.height = h;
	mainFboDesc.colorAttachments = { {mData.positionBuffer}, {mData.normalBuffer}, {mData.albedoBuffer} };

	TextureDesc depthTex{};
	depthTex.width = w;
	depthTex.height = h;
	depthTex.depth = 1;
	depthTex.dimension = TextureDimension::Tex2D;
	depthTex.format = TextureFormat::DEPTH_COMPONENT24;
	depthTex.usage = TextureUsage::DepthStencilAttachment;

	mData.depthsBuffer = rhi->createTexture(depthTex);
	mainFboDesc.depthStencilAttachment = FramebufferAttachmentDesc{.texture = mData.depthsBuffer };

	mData.gFramebuffer = rhi->createFramebuffer(mainFboDesc);

	FramebufferDesc outlineFboDesc;
	outlineFboDesc.width = w;
	outlineFboDesc.height = h;
	outlineFboDesc.colorAttachments = { {mData.outlinesBuffer} };
	mData.outlineFramebuffer = rhi->createFramebuffer(outlineFboDesc);

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
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::Back;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.fillMode = PolygonFillMode::Fill;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = true;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::Less;
	pipelineDesc.blend.enabled = false;
	pipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Vertex,
		"g_buffer.vs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_buffer.vs")
	});
	pipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Fragment,
		"g_buffer.fs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_buffer.fs")
		});

	geometryPipeline = Engine::instance()->rhi()->createGraphicsPipeline(pipelineDesc);
	mGBufferPipelineDesc = pipelineDesc;
	mGBufferPipelines.emplace(std::string(ComponentsModule::DefaultGBufferVS) + "|" + ComponentsModule::DefaultGBufferFS, geometryPipeline);

	RenderPassDesc geometryPassDesc;
	geometryPassDesc.colorAttachmentCount = 3;
	geometryPassDesc.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //position
	geometryPassDesc.colorAttachments[1] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //normal
	geometryPassDesc.colorAttachments[2] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //albedo
	geometryPassDesc.hasDepthStencil = true;
	geometryPassDesc.depthAttachment = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, 1.f, 0, false };

	geometryPass = Engine::instance()->rhi()->createRenderPass(geometryPassDesc);


	GraphicsPipelineDesc gBufferOutlinePDesc;
	gBufferOutlinePDesc.layout = layout;
	gBufferOutlinePDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Vertex,
		"g_buffer_outlines.vs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_buffer_outlines.vs")
		});
	gBufferOutlinePDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Fragment,
		"g_buffer_outlines.fs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_buffer_outlines.fs")
		});
	gBufferOutlinePDesc.topology = PrimitiveTopology::Triangles;
	gBufferOutlinePDesc.cullMode = CullMode::Back;
	gBufferOutlinePipeline = Engine::instance()->rhi()->createGraphicsPipeline(gBufferOutlinePDesc);

	RenderPassDesc gbrp;
	gbrp.colorAttachmentCount = 1;
	gbrp.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 0.f} }; //outlines
	outlineBufferRenderPass = Engine::instance()->rhi()->createRenderPass(gbrp);


	GraphicsPipelineDesc outlinePDesc;
	outlinePDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Vertex,
		"g_outline.vs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_outline.vs")
		});
	outlinePDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({
		ShaderStage::Fragment,
		"g_outline.fs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/g_outline.fs")
		});

	outlinePDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	outlinePDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	outlinePDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);

	outlinePipeline = Engine::instance()->rhi()->createGraphicsPipeline(outlinePDesc);

	RenderPassDesc outlineRPD;
	outlineRPD.colorAttachmentCount = 1;
	outlineRPD.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 0.f} }; //final outline
	outlineRenderPass = Engine::instance()->rhi()->createRenderPass(outlineRPD);

	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(25, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(26, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(27, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Fragment);
	mTexSetLayoutOutlines = rhi->createDescriptorSetLayout(texLayoutDesc);
	mTexSetOutlines = rhi->allocateDescriptorSet(mTexSetLayoutOutlines);

	Render::DescriptorSetLayoutDesc texLayoutDescGBuf;
	texLayoutDescGBuf.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(5, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDescGBuf.bindings.emplace_back(3, Render::DescriptorType::StorageBuffer, Render::ShaderStage::Fragment);
	gBuffetLayout = rhi->createDescriptorSetLayout(texLayoutDescGBuf);
	gBuffetLayoutSet = rhi->allocateDescriptorSet(gBuffetLayout);

	initGrid();
	publish(renderDataHandle);
}

void GeometryPass::publish(SystemsModule::RenderData& renderDataHandle) {
	auto& frame = renderDataHandle.frame;
	const auto s = mData.sampler;
	frame.set(FrameId::GBufferFbo, { .framebuffer = mData.gFramebuffer });
	frame.set(FrameId::GBufferOutlineFbo, { .framebuffer = mData.outlineFramebuffer });
	frame.set(FrameId::GBufferPosition, { .texture = mData.positionBuffer, .sampler = s });
	frame.set(FrameId::GBufferNormal, { .texture = mData.normalBuffer, .sampler = s });
	frame.set(FrameId::GBufferAlbedo, { .texture = mData.albedoBuffer, .sampler = s });
	frame.set(FrameId::GBufferOutlines, { .texture = mData.outlinesBuffer, .sampler = s });
	frame.set(FrameId::GBufferLights, { .texture = mData.lightsBuffer, .sampler = s });
	frame.set(FrameId::GBufferDepth, { .texture = mData.depthsBuffer, .sampler = s });
}

void GeometryPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!mInited) {
		return;
	}

	//if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		//todo make rhi way to set polygon mode via another pipeline state object or dynamic state
		//PolygonMode<PolygonFace::FRONT_AND_BACK>::push(PolygonType::LINE);
	//}

	publish(renderDataHandle);
	{
		const auto curPassData = getContainer().getCurrentPassData();
		curPassData->waitReady();
	}

	const auto curPassData = getContainer().getCurrentPassData();
	const auto outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();
	auto rhi = Engine::instance()->rhi();
	RenderPassBeginInfo geometryPassBegin;
	geometryPassBegin.renderPass = geometryPass;
	geometryPassBegin.framebuffer = mData.gFramebuffer;
	geometryPassBegin.width = Engine::instance()->getWindow()->getScreenData().renderW;
	geometryPassBegin.height = Engine::instance()->getWindow()->getScreenData().renderH;

	rhi->cmdBeginRenderPass(cmd, geometryPassBegin);
	if (!curPassData->getBatcher().drawList.empty()) {
		curPassData->getBatcher().flushAll(cmd, gBuffetLayoutSet, [this](const std::string& vs, const std::string& fs) {
			return getGBufferPipeline(vs, fs);
		});
	}
	drawGrid(renderDataHandle, cmd);
	rhi->cmdEndRenderPass(cmd);

	/*if (!batcher.drawList.empty()) {
		auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
		lightObjectsPass->use();
		batcher.flushAll(true);
	}*/

	if (!outlineData->getBatcher().drawList.empty()) {
		RenderPassBeginInfo beginGBuf;
		beginGBuf.renderPass = outlineBufferRenderPass;
		beginGBuf.framebuffer = mData.outlineFramebuffer;
		beginGBuf.width = Engine::instance()->getWindow()->getScreenData().renderW;
		beginGBuf.height = Engine::instance()->getWindow()->getScreenData().renderH;

		needClearOutlines = true;

		rhi->cmdBeginRenderPass(cmd, beginGBuf);
		rhi->cmdBindPipeline(cmd, gBufferOutlinePipeline);
		outlineData->getBatcher().flushAll(cmd, mTexSetOutlines);
		rhi->cmdEndRenderPass(cmd);

		RenderPassBeginInfo beginOutlines;
		beginOutlines.renderPass = outlineRenderPass;
		beginOutlines.width = Engine::instance()->getWindow()->getScreenData().renderW;
		beginOutlines.height = Engine::instance()->getWindow()->getScreenData().renderH;
		rhi->cmdBeginRenderPass(cmd, beginOutlines);
		rhi->cmdBindPipeline(cmd, outlinePipeline);


		std::vector<Render::WriteDescriptor> writes;

		{
			Render::WriteDescriptor w{};
			w.set = mTexSetOutlines;
			w.binding = 25;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mData.lightsBuffer, mData.sampler });
			writes.push_back(std::move(w));
		}

		{
			Render::WriteDescriptor w{};
			w.set = mTexSetOutlines;
			w.binding = 26;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mData.normalBuffer, mData.sampler });
			writes.push_back(std::move(w));
		}

		{
			Render::WriteDescriptor w{};
			w.set = mTexSetOutlines;
			w.binding = 27;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mData.outlinesBuffer, mData.sampler });

			writes.push_back(std::move(w));
		}

		if (!writes.empty()) {
			rhi->updateDescriptorSets(writes);
		}
		rhi->cmdBindDescriptorSets(cmd, outlinePipeline, 0, &mTexSetOutlines, 1);

		Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}
	else {
		if (needClearOutlines) {
			RenderPassBeginInfo beginGBuf;
			beginGBuf.renderPass = outlineBufferRenderPass;
			beginGBuf.framebuffer = mData.outlineFramebuffer;
			beginGBuf.width = Engine::instance()->getWindow()->getScreenData().renderW;
			beginGBuf.height = Engine::instance()->getWindow()->getScreenData().renderH;
			rhi->cmdBeginRenderPass(cmd, beginGBuf);
			rhi->cmdEndRenderPass(cmd);

			needClearOutlines = false;
		}
	}
		
	//if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
	//	PolygonMode<PolygonFace::FRONT_AND_BACK>::pop();
	//}
}

SFE::Render::PipelineHandle GeometryPass::getGBufferPipeline(const std::string& vertexShader, const std::string& fragmentShader) {
	const std::string vs = vertexShader.empty() ? ComponentsModule::DefaultGBufferVS : vertexShader;
	const std::string fs = fragmentShader.empty() ? ComponentsModule::DefaultGBufferFS : fragmentShader;
	const auto key = vs + "|" + fs;
	if (const auto it = mGBufferPipelines.find(key); it != mGBufferPipelines.end()) {
		return it->second;
	}

	auto* rhi = Engine::instance()->rhi();
	const auto vertCode = ShaderModule::ShaderBase::loadShaderCode(vs);
	const auto fragCode = ShaderModule::ShaderBase::loadShaderCode(fs);
	if (vertCode.empty() || fragCode.empty()) {
		LogsModule::Logger::LOG_WARNING("GBuffer shader missing: %s / %s", vs.c_str(), fs.c_str());
		return geometryPipeline;
	}

	auto desc = mGBufferPipelineDesc;
	desc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, vs, vertCode });
	desc.shaders.fragment = rhi->createShaderModule({ ShaderStage::Fragment, fs, fragCode });
	const auto pipeline = rhi->createGraphicsPipeline(desc);
	mGBufferPipelines.emplace(key, pipeline);
	return pipeline;
}

void GeometryPass::initGrid() {
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
	pipelineDesc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, "grid vs", ShaderModule::ShaderBase::loadShaderCode("shaders/testSh.vs") });
	pipelineDesc.shaders.fragment = rhi->createShaderModule({ ShaderStage::Fragment, "grid fs", ShaderModule::ShaderBase::loadShaderCode("shaders/testSh.fs") });
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
	rhi->cmdBindIndexBuffer(cmd, mGridEBO, IndexType::Uint32);
	rhi->cmdBindVertexBuffer(cmd, 0, mGridVBO);
	mGridDrawData.viewPos.xyz = renderDataHandle.mCameraPos;
	mGridDrawData.far = cameraComp->getProjection().getFar();
	mGridDrawData.near = cameraComp->getProjection().getNear();
	rhi->cmdPushConstants(cmd, mGridPipeline, mGridDrawData);
	rhi->cmdDrawIndexed(cmd, 6);
}
