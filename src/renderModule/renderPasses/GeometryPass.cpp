#include "GeometryPass.h"

#include "imgui.h"
#include "componentsModule/ModelComponent.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/ShaderComponent.h"
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
	curPassData->mStatus = RenderPreparingStatus::PREPARING;

	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	ThreadPool::instance()->addTask<WorkerType::RENDER>([nextRegistry = renderData.nextRegistry, this, curPassData, camFrustum = renderData.mNextCamFrustum, camPos = renderData.mCameraPos, outlineData]() mutable {
		FUNCTION_BENCHMARK;
		curPassData->getBatcher().clear();
		outlineData->getBatcher().clear();

		ecss::Ranges<ecss::EntityId> entities;
		{
			FUNCTION_BENCHMARK_NAMED(GeometryPass_octree);
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			for (auto& treePos : octreeSys->getAABBOctrees(camFrustum.generateAABB())) {
				if (const auto tree = octreeSys->getOctree(treePos)) {
					auto lock = tree->readLock();
					tree->forEachObjectInsideFrustum(camFrustum, [&entities](auto id) {
						entities.insert(id);
					});
				}
			}
		}

		if (entities.empty()) {
			curPassData->mStatus = RenderPreparingStatus::READY;
			return;
		}

		{
			FUNCTION_BENCHMARK_NAMED(addedToBatcher);
			auto& batcher = curPassData->getBatcher();
			for (auto [ent, transform, meshComp, matComp, armatComp/*, oclComp*/] : ECSHandler::drawRegistry(nextRegistry).view<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent, const ComponentsModule::ArmatureBonesComponent/*, const ComponentsModule::OccludedComponent*/>(entities)) {
				if (!meshComp) {
					continue;
				}
				/*if (oclComp && oclComp->occluded) {
					continue;
				}*/
				for (const auto& mesh : meshComp->meshGraph) {
					batcher.addToDrawList(mesh.value, {}, ent);
				}

				/*for (const auto& mesh : meshComp->meshGraph) {
					batcher.addToDrawList(mesh.value.vaoId, mesh.value.verticesCount, mesh.value.indicesCount, matComp ? matComp->materials : ComponentsModule::Materials{}, transform->mTransform, armatComp ? const_cast<Math::Mat4*>(armatComp->boneMatrices.data()) : nullptr);
				}*/
			}
			batcher.sort(camPos);
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
					outlineBatcher.addToDrawList(mesh.value, {}, entity);
				}
			}

			//outlineBatcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());
		}
		curPassData->mStatus = RenderPreparingStatus::READY;
	});
}

void GeometryPass::init() {
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

	// viewPos buffer
	TextureDesc viewPositionBufDesc;
	viewPositionBufDesc.width = w;
	viewPositionBufDesc.height = h;
	viewPositionBufDesc.format = TextureFormat::RGBA32F;
	mData.viewPositionBuffer = rhi->createTexture(viewPositionBufDesc);

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
	mainFboDesc.colorAttachments = { {mData.positionBuffer}, {mData.normalBuffer},{mData.albedoBuffer}, {mData.viewPositionBuffer}/*, {mData.outlinesBuffer}, {mData.lightsBuffer}*/ };

	TextureDesc depthTex{};
	depthTex.width = w;
	depthTex.height = h;
	depthTex.depth = 1;
	depthTex.dimension = TextureDimension::Tex2D;
	depthTex.format = TextureFormat::DEPTH_COMPONENT24;
	depthTex.usage = TextureUsage::DepthStencilAttachment;

	TextureHandle depthHandle = rhi->createTexture(depthTex);
	mainFboDesc.depthStencilAttachment = {.texture = depthHandle };

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
	pipelineDesc.frontFace = FrontFace::CCW;
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

	RenderPassDesc geometryPassDesc;
	geometryPassDesc.colorAttachmentCount = 4;
	geometryPassDesc.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //position
	geometryPassDesc.colorAttachments[1] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //normal
	geometryPassDesc.colorAttachments[2] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //albedo
	geometryPassDesc.colorAttachments[3] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {0.f, 0.f, 0.f, 1.f} }; //viewPos
	geometryPassDesc.hasDepthStencil = true;
	geometryPassDesc.depthAttachment = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, 1.f, 0, false };

	geometryPass = Engine::instance()->rhi()->createRenderPass(geometryPassDesc);
}

void GeometryPass::render(SystemsModule::RenderData& renderDataHandle) {
	if (!mInited) {
		return;
	}
	FUNCTION_BENCHMARK
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		GLW::PolygonMode<GLW::PolygonFace::FRONT_AND_BACK>::push(GLW::PolygonType::LINE);
	}


	renderDataHandle.mGeometryPassData = &mData;
	{
		FUNCTION_BENCHMARK_NAMED(_wait_lock);
		const auto curPassData = getContainer().getCurrentPassData();
		while (curPassData->mStatus != RenderPreparingStatus::READY) {}
	}

	const auto curPassData = getContainer().getCurrentPassData();
	const auto outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();

	if (!curPassData->getBatcher().drawList.empty()) {
		RenderPassBeginInfo geometryPassBegin;
		geometryPassBegin.renderPass = geometryPass;
		geometryPassBegin.framebuffer = mData.gFramebuffer;
		geometryPassBegin.width = Engine::instance()->getWindow()->getScreenData().renderW;
		geometryPassBegin.height = Engine::instance()->getWindow()->getScreenData().renderH;
		Engine::instance()->rhi()->beginRenderPass(geometryPassBegin);
		geometryPipeline.bind();
		Engine::instance()->rhi()->setUniform(geometryPipeline, "texture_diffuse1", static_cast<int>(SFE::DIFFUSE));
		Engine::instance()->rhi()->setUniform(geometryPipeline, "normalMap", static_cast<int>(SFE::NORMALS));
		Engine::instance()->rhi()->setUniform(geometryPipeline, "texture_specular1", static_cast<int>(SFE::SPECULAR));

		Engine::instance()->rhi()->setUniform(geometryPipeline, "outline", false);

		FUNCTION_BENCHMARK_NAMED(_flush)
		curPassData->getBatcher().flushAll();
		Engine::instance()->rhi()->endRenderPass();
	}

	/*if (!batcher.drawList.empty()) {
		auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
		lightObjectsPass->use();
		batcher.flushAll(true);
	}*/

	if (!outlineData->getBatcher().drawList.empty()) {
		needClearOutlines = true;
		Engine::instance()->rhi()->bindFramebuffer(mData.outlineFramebuffer);
		//mData.outlinesBuffer.create();
		//mData.outlineFramebuffer.addAttachmentTexture(0, &mData.outlinesBuffer); //todo hack to clear image
		//mData.outlineFramebuffer.finalize();

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();
		g_buffer_outlines->setUniform("PV", renderDataHandle.current.PV);

		outlineData->getBatcher().flushAll();

		Engine::instance()->rhi()->bindFramebuffer({});
		Engine::instance()->rhi()->bindTexture(26, mData.normalBuffer, mData.sampler);
		Engine::instance()->rhi()->bindTexture(27, mData.outlinesBuffer, mData.sampler);
		Engine::instance()->rhi()->bindTexture(25, mData.lightsBuffer, mData.sampler);

		auto outlineG = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_outline.vs", "shaders/g_outline.fs");
		outlineG->use();
		outlineG->setUniform("gDepth", 26);
		outlineG->setUniform("gOutlinesP", 27);
		outlineG->setUniform("gLightsP", 25);

		Utils::renderQuad();
	}
	else {
		if (needClearOutlines) {
			needClearOutlines = false;
			Engine::instance()->rhi()->bindFramebuffer(mData.outlineFramebuffer);
			GLW::clear(GLW::ColorBit::ALL);
			//mData.outlinesBuffer.create();
		}
	}

	//Engine::instance()->rhi()->bindFramebuffer({});
	
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		GLW::PolygonMode<GLW::PolygonFace::FRONT_AND_BACK>::pop();
	}
}
