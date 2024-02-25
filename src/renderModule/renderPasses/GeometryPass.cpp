#include "GeometryPass.h"

#include "imgui.h"
#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/ShaderComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"
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
	//mStatus = RenderPreparingStatus::PREPARING;

	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	currentLock = ThreadPool::instance()->addTask<WorkerType::RENDER>([this, curPassData, camFrustum = renderData.mNextCamFrustum, camPos = renderData.mCameraPos, entities = std::vector<unsigned>(), outlineData]() mutable {
		FUNCTION_BENCHMARK
		curPassData->getBatcher().drawList.clear();

		{
			FUNCTION_BENCHMARK_NAMED(octree);
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			std::mutex addMtx;
			auto aabbOctrees = octreeSys->getAABBOctrees(camFrustum.generateAABB());
			ThreadPool::instance()->addBatchTasks(aabbOctrees.size(), 5, [aabbOctrees, octreeSys, &addMtx, &camFrustum, &entities](size_t it) {
				if (auto treeIt = octreeSys->getOctree(aabbOctrees[it])) {
					auto lock = treeIt->readLock();
					treeIt->forEachObjectInFrustum(camFrustum, [&entities, &camFrustum, &addMtx](const auto& obj) {
						if (FrustumModule::AABB::isOnFrustum(camFrustum, obj.pos, obj.size)) {
							std::unique_lock lock(addMtx);
							entities.emplace_back(obj.data.getID());
						}
					});
				}
				
			}).waitAll();
		}

		if (entities.empty()) {
			return;
		}
		std::ranges::sort(entities);

		{
			auto& batcher = curPassData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcher)
			for (auto [ent, transform, modelComp, animComp] : ECSHandler::registry().getComponentsArray<TransformComponent, ModelComponent, ComponentsModule::AnimationComponent>(entities)) {
				if (!modelComp) {
					continue;
				}

				for (auto& mesh : modelComp->getModel().meshes) {
					batcher.addToDrawList(mesh->getVAO(), mesh->mData.vertices.size(), mesh->mData.indices.size(), mesh->mMaterial, transform->getTransform(), modelComp->boneMatrices, false);
				}
			}
			batcher.sort(camPos);
		}

		{
			auto& outlineBatcher = outlineData->getBatcher();
			FUNCTION_BENCHMARK_NAMED(addedToBatcherOutline)
			for (const auto& [entity, outline, transform, modelComp, animComp] : ECSHandler::registry().getComponentsArray<OutlineComponent, TransformComponent, ModelComponent, ComponentsModule::AnimationComponent>()) {
				if (!modelComp || !transform) {
					continue;
				}

				if (std::find(entities.begin(), entities.end(), entity) == entities.end()) {
					continue;
				}
			
				for (auto& mesh : modelComp->getModel().meshes) {
					outlineBatcher.addToDrawList(mesh->getVAO(), mesh->mData.vertices.size(), mesh->mData.indices.size(), mesh->mMaterial, transform->getTransform(), modelComp->boneMatrices, false);
				}
			}

			outlineBatcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());
		}
	});
}

void GeometryPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mOutlineData.init(2);
	getContainer().init(2);


	// position color buffer
	const auto w = Renderer::SCR_RENDER_W;
	const auto h = Renderer::SCR_RENDER_H;

	mData.positionBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.positionBuffer->image2D(w, h, AssetsModule::RGBA32F, AssetsModule::RGBA, AssetsModule::FLOAT);

	mData.positionBuffer->setParameter({ 
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	// normal color buffer
	mData.normalBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.normalBuffer->image2D(w, h, AssetsModule::RGBA16F, AssetsModule::RGBA, AssetsModule::FLOAT);
	mData.normalBuffer->setParameter({
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	// color + specular color buffer
	mData.albedoBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.albedoBuffer->image2D(w, h, AssetsModule::RGBA16F, AssetsModule::RGBA, AssetsModule::UNSIGNED_BYTE);
	mData.albedoBuffer->setParameter({
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	// viewPos buffer
	mData.viewPositionBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.viewPositionBuffer->image2D(w, h, AssetsModule::RGBA32F, AssetsModule::RGBA, AssetsModule::UNSIGNED_BYTE);
	mData.viewPositionBuffer->setParameter({
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	// outline buffer
	mData.outlinesBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.outlinesBuffer->image2D(w, h, AssetsModule::RGBA8, AssetsModule::RGBA, AssetsModule::UNSIGNED_BYTE);
	mData.outlinesBuffer->setParameter({
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	// light buffer
	mData.lightsBuffer = new AssetsModule::Texture(AssetsModule::TEXTURE_2D);
	mData.lightsBuffer->image2D(w, h, AssetsModule::RGBA8, AssetsModule::RGBA, AssetsModule::UNSIGNED_BYTE);
	mData.lightsBuffer->setParameter({
		{AssetsModule::TEXTURE_MIN_FILTER, GL_NEAREST},
		{AssetsModule::TEXTURE_MAG_FILTER, GL_NEAREST}
	});

	mData.gFramebuffer = new Render::Framebuffer();
	mData.gFramebuffer->bind();

	mData.gFramebuffer->addAttachmentTexture(0, mData.positionBuffer);
	mData.gFramebuffer->addAttachmentTexture(1, mData.normalBuffer);
	mData.gFramebuffer->addAttachmentTexture(2, mData.albedoBuffer);
	mData.gFramebuffer->addAttachmentTexture(3, mData.viewPositionBuffer);
	mData.gFramebuffer->addAttachmentTexture(4, mData.outlinesBuffer);
	mData.gFramebuffer->addAttachmentTexture(5, mData.lightsBuffer);

	// create and attach depth buffer (renderbuffer)
	glGenRenderbuffers(1, &mData.rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mData.rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);

	mData.gFramebuffer->addRenderbuffer(GL_DEPTH_ATTACHMENT, mData.rboDepth);
	mData.gFramebuffer->finalize();

	mData.outlineFramebuffer = new Render::Framebuffer();
	mData.outlineFramebuffer->bind();

	mData.outlineFramebuffer->addAttachmentTexture(0, mData.outlinesBuffer);

	mData.outlineFramebuffer->finalize();

	Render::Framebuffer::bindDefaultFramebuffer();
}

void GeometryPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	if (!mInited) {
		return;
	}
	FUNCTION_BENCHMARK
	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}


	renderDataHandle.mGeometryPassData = mData;
	if (currentLock.valid()) {
		FUNCTION_BENCHMARK_NAMED(_lock_wait)
		currentLock.wait();
	}

	const auto curPassData = getContainer().getCurrentPassData();
	const auto outlineData = mOutlineData.getCurrentPassData();
	getContainer().rotate();
	mOutlineData.rotate();
	prepare();

	glViewport(0, 0, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);
	mData.gFramebuffer->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!curPassData->getBatcher().drawList.empty()) {
		auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
		shaderGeometryPass->use();
		shaderGeometryPass->setInt("texture_diffuse1", AssetsModule::DIFFUSE);
		shaderGeometryPass->setInt("normalMap", AssetsModule::NORMALS);
		shaderGeometryPass->setInt("texture_specular1", AssetsModule::SPECULAR);
		shaderGeometryPass->setBool("outline", false);

		curPassData->getBatcher().flushAll(true);
	}

	if (!batcher.drawList.empty()) {
		auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
		lightObjectsPass->use();
		batcher.flushAll(true);
	}

	if (!outlineData->getBatcher().drawList.empty()) {
		needClearOutlines = true;
		mData.outlineFramebuffer->bind();
		//glClear(GL_COLOR_BUFFER_BIT);

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();
		g_buffer_outlines->setMat4("PV", renderDataHandle.current.PV);

		outlineData->getBatcher().flushAll(true);

		mData.outlineFramebuffer->bind();

		auto outlineG = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_outline.vs", "shaders/g_outline.fs");
		outlineG->use();
		AssetsModule::TextureHandler::instance()->bindTextureToSlot(26, mData.normalBuffer);
		AssetsModule::TextureHandler::instance()->bindTextureToSlot(27, mData.outlinesBuffer);
		AssetsModule::TextureHandler::instance()->bindTextureToSlot(25, mData.lightsBuffer);
		outlineG->setInt("gDepth", 26);
		outlineG->setInt("gOutlinesP", 27);
		outlineG->setInt("gLightsP", 25);

		Utils::renderQuad();
	}
	else {
		if (needClearOutlines) {
			needClearOutlines = false;

			mData.outlineFramebuffer->bind();
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	Render::Framebuffer::bindDefaultFramebuffer();

	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
}
