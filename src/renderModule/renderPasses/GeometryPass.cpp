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

		SFE::Vector<ecss::EntityId> entities;
		{
			FUNCTION_BENCHMARK_NAMED(GeometryPass_octree);
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			for (auto& treePos : octreeSys->getAABBOctrees(camFrustum.generateAABB())) {
				if (const auto tree = octreeSys->getOctree(treePos)) {
					auto lock = tree->readLock();
					tree->forEachObjectInFrustum(camFrustum, [&entities, &camFrustum](const auto& obj, bool entirely) {
						if (entirely || FrustumModule::AABB::isOnFrustum(camFrustum, obj.pos, obj.size)) {
							entities.emplace_back(obj.data);
						}
					});
				}
			}
		}

		if (entities.empty()) {
			curPassData->mStatus = RenderPreparingStatus::READY;
			return;
		}
		entities.sort();
		entities.removeDuplicatesSorted();

		{
			FUNCTION_BENCHMARK_NAMED(addedToBatcher);
			auto& batcher = curPassData->getBatcher();
			for (auto [ent, transform, meshComp, matComp, oclComp] : ECSHandler::drawRegistry(nextRegistry).forEach<const ComponentsModule::TransformMatComp, const MeshComponent, const MaterialComponent, const ComponentsModule::OccludedComponent>({entities}, false)) {
				if (!meshComp) {
					continue;
				}
				if (oclComp && oclComp->occluded) {
					continue;
				}

				for (const auto& mesh : meshComp->meshGraph) {
					batcher.addToDrawList(ent, mesh.value.vaoId, mesh.value.verticesCount, mesh.value.indicesCount, matComp ? matComp->materials : ComponentsModule::Materials{}, transform->mTransform);
				}
			}
			batcher.sort(camPos);
		}

		{
			auto& outlineBatcher = outlineData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcherOutline)
			for (const auto& [entity, outline, transform, meshComp] : ECSHandler::drawRegistry(nextRegistry).forEach<const OutlineComponent, const ComponentsModule::TransformMatComp, const MeshComponent>({}, false)) {
				if (!meshComp) {
					continue;
				}
				if (!entities.containsSorted(entity)) {//need to check - if draw all outline objects is faster then cull all of them such way?//todo
					continue;
				}

				for (const auto& mesh : meshComp->meshGraph) {
					outlineBatcher.addToDrawList(entity, mesh.value.vaoId, mesh.value.verticesCount, mesh.value.indicesCount, {}, transform->mTransform);
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

	// position color buffer
	mData.positionBuffer.width = w;
	mData.positionBuffer.height = h;
	mData.positionBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.positionBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.positionBuffer.pixelFormat = GLW::RGBA32F;
	mData.positionBuffer.textureFormat = GLW::RGBA;
	mData.positionBuffer.pixelType = GLW::FLOAT;
	mData.positionBuffer.create();
	
	mData.normalBuffer.width = w;
	mData.normalBuffer.height = h;
	mData.normalBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.normalBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.normalBuffer.pixelFormat = GLW::RGBA16F;
	mData.normalBuffer.textureFormat = GLW::RGBA;
	mData.normalBuffer.pixelType = GLW::FLOAT;
	mData.normalBuffer.create();

	// color + specular color buffer
	mData.albedoBuffer.width = w;
	mData.albedoBuffer.height = h;
	mData.albedoBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.albedoBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.albedoBuffer.pixelFormat = GLW::RGBA16F;
	mData.albedoBuffer.textureFormat = GLW::RGBA;
	mData.albedoBuffer.pixelType = GLW::UNSIGNED_BYTE;
	mData.albedoBuffer.create();

	// viewPos buffer
	mData.viewPositionBuffer.width = w;
	mData.viewPositionBuffer.height = h;
	mData.viewPositionBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.viewPositionBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.viewPositionBuffer.pixelFormat = GLW::RGBA32F;
	mData.viewPositionBuffer.textureFormat = GLW::RGBA;
	mData.viewPositionBuffer.pixelType = GLW::UNSIGNED_BYTE;
	mData.viewPositionBuffer.create();

	// outline buffer
	mData.outlinesBuffer.width = w;
	mData.outlinesBuffer.height = h;
	mData.outlinesBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.outlinesBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.outlinesBuffer.pixelFormat = GLW::RGBA8;
	mData.outlinesBuffer.textureFormat = GLW::RGBA;
	mData.outlinesBuffer.pixelType = GLW::UNSIGNED_BYTE;
	mData.outlinesBuffer.create();

	// light buffer
	mData.lightsBuffer.width = w;
	mData.lightsBuffer.height = h;
	mData.lightsBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.lightsBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.lightsBuffer.pixelFormat = GLW::RGBA8;
	mData.lightsBuffer.textureFormat = GLW::RGBA;
	mData.lightsBuffer.pixelType = GLW::UNSIGNED_BYTE;
	mData.lightsBuffer.create();

	mData.gFramebuffer.bind();

	mData.gFramebuffer.addAttachmentTexture(0, &mData.positionBuffer);
	mData.gFramebuffer.addAttachmentTexture(1, &mData.normalBuffer);
	mData.gFramebuffer.addAttachmentTexture(2, &mData.albedoBuffer);
	mData.gFramebuffer.addAttachmentTexture(3, &mData.viewPositionBuffer);
	//mData.gFramebuffer.addAttachmentTexture(4, &mData.outlinesBuffer);
	//mData.gFramebuffer.addAttachmentTexture(5, &mData.lightsBuffer);

	// create and attach depth buffer (renderbuffer)
	mData.rboDepth.generate();
	mData.rboDepth.bind();
	mData.rboDepth.storage(GLW::DEPTH_COMPONENT24, w, h);

	mData.gFramebuffer.addRenderbuffer(GLW::AttachmentType::DEPTH, mData.rboDepth.id);
	mData.gFramebuffer.finalize();

	mData.outlineFramebuffer.bind();
	mData.outlineFramebuffer.addAttachmentTexture(0, &mData.outlinesBuffer);
	mData.outlineFramebuffer.finalize();

	GLW::Framebuffer::bindDefaultFramebuffer();
	GLW::Framebuffer::bindDefaultFramebuffer();
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
		while (curPassData->mStatus != RenderPreparingStatus::READY) {}//todo do some logic on main thread while waiting preparing
	}

	const auto curPassData = getContainer().getCurrentPassData();
	const auto outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();

	mData.gFramebuffer.bind();
	GLW::clear(GLW::ColorBit::DEPTH_COLOR);
	
	if (!curPassData->getBatcher().drawList.empty()) {
		auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
		shaderGeometryPass->use();
		shaderGeometryPass->setUniform<int>("texture_diffuse1", SFE::DIFFUSE);
		shaderGeometryPass->setUniform<int>("normalMap", SFE::NORMALS);
		shaderGeometryPass->setUniform<int>("texture_specular1", SFE::SPECULAR);
		shaderGeometryPass->setUniform("outline", false);

		FUNCTION_BENCHMARK_NAMED(_flush)
		curPassData->getBatcher().flushAll();
	}

	if (!outlineData->getBatcher().drawList.empty()) {
		needClearOutlines = true;

		mData.outlineFramebuffer.bind();
		GLW::clear(GLW::ColorBit::COLOR);

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();

		outlineData->getBatcher().flushAll();
		bindTextureToSlot(26, &mData.normalBuffer);
		bindTextureToSlot(27, &mData.outlinesBuffer);
		bindTextureToSlot(25, &mData.lightsBuffer);

		auto outlineG = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_outline.vs", "shaders/g_outline.fs");
		outlineG->use();
		outlineG->setUniform("gDepth", 26);
		outlineG->setUniform("gOutlinesP", 27);
		outlineG->setUniform("gLightsP", 25);

		Utils::renderQuad();
		GLW::Framebuffer::bindDefaultFramebuffer();
	}
	else {
		if (needClearOutlines) {
			needClearOutlines = false;

			mData.outlineFramebuffer.bind();
			GLW::clear(GLW::ColorBit::COLOR);
			GLW::Framebuffer::bindDefaultFramebuffer();
		}
	}

	
	GLW::Framebuffer::bindDefaultFramebuffer();
	
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		GLW::PolygonMode<GLW::PolygonFace::FRONT_AND_BACK>::pop();
	}
}
