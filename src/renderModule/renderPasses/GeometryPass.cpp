#include "GeometryPass.h"

#include "imgui.h"
#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
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

using namespace Engine::RenderModule::RenderPasses;

void GeometryPass::prepare() {
	auto curPassData = getCurrentPassData();
	mStatus = RenderPreparingStatus::PREPARING;

	auto& renderData = ECSHandler::getSystem<Engine::SystemsModule::RenderSystem>()->getRenderData();
	currentLock = ThreadPool::instance()->addTask<WorkerType::RENDER>([this, curPassData, camFrustum = renderData.mNextCamFrustum, camPos = renderData.mCameraPos, entities = std::vector<unsigned>()]() mutable {
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
			for (auto [ent, transform, modelComp] : ECSHandler::registry().getComponentsArray<TransformComponent, ModelComponent>(entities)) {
				if (!&modelComp) {
					continue;
				}

				for (auto& mesh : modelComp.getModel().mMeshHandles) {
					batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
				}
			}
			batcher.sort(camPos);
		}

		mStatus = RenderPreparingStatus::READY;
	});
}

void GeometryPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;


	passData.push_back(new RenderPassData());
	passData.push_back(new RenderPassData());

	// configure g-buffer framebuffer
	// ------------------------------

	glGenFramebuffers(1, &mData.mGBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);

	// position color buffer
	glGenTextures(1, &mData.gPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gPosition, 0);

	// normal color buffer
	glGenTextures(1, &mData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gNormal, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gAlbedoSpec);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mData.gAlbedoSpec, 0);

	// viewPos buffer
	glGenTextures(1, &mData.gViewPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gViewPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mData.gViewPosition, 0);

	glGenTextures(1, &mData.gOutlines);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gOutlines);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, mData.gOutlines, 0);

	glGenTextures(1, &mData.gLights);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, mData.gLights, 0);


	// Attach a depth texture to the framebuffer

	glGenTextures(1, &mData.gDepthTexture);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mData.gDepthTexture, 0);


	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
	constexpr unsigned int attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
	glDrawBuffers(6, attachments);

	// create and attach depth buffer (renderbuffer)
	glGenRenderbuffers(1, &mData.rboDepth);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mData.rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mData.rboDepth);

	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	glGenFramebuffers(1, &mOData.mFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);

	// outlines buffer
	//glGenTextures(1, &mData.gOutlines);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gOutlines);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gOutlines, 0);

	/*AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gLights);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gLights, 0);*/


	constexpr unsigned int Oattachments[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, Oattachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	const auto curPassData = getCurrentPassData();
	rotate();
	prepare();

	glViewport(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
	shaderGeometryPass->use();
   
    shaderGeometryPass->setMat4("view",  renderDataHandle.current.view);
    shaderGeometryPass->setMat4("PV",  renderDataHandle.current.PV);
	shaderGeometryPass->setInt("texture_diffuse1", 0);
	shaderGeometryPass->setInt("normalMap", 1);
	shaderGeometryPass->setBool("outline", false);

	curPassData->getBatcher().flushAll(true);

	/*for (const auto& [entt, lightSource, transform, modelComp, aabb] : ECSHandler::registry().getComponentsArray<LightSourceComponent, TransformComponent, ModelComponent, ComponentsModule::AABBComponent>()) {
		if (!&modelComp) {
			continue;
		}
		int i = 0;
		for (auto& mesh : modelComp.getModel().mMeshHandles) {
			if (aabb.aabbs[i].isOnFrustum(renderDataHandle.mCamFrustum)) {
				batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
			}
			i++;
		}
		batcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());
	}*/

	if (!batcher.drawList.empty()) {
		auto lightObjectsPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_light.vs", "shaders/g_buffer_light.fs");
		lightObjectsPass->use();
		batcher.flushAll(true);
	}
	

	
	auto outlineNodes = ECSHandler::registry().getComponentsArray<OutlineComponent>();
	if (!outlineNodes.empty()) {
		needClearOutlines = true;

		glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);
		//glClear(GL_COLOR_BUFFER_BIT);

		auto g_buffer_outlines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer_outlines.vs", "shaders/g_buffer_outlines.fs");
		g_buffer_outlines->use();
		g_buffer_outlines->setMat4("PV", renderDataHandle.current.PV);

		for (const auto& [entity, outline, transform, modelComp, aabbcomp] : ECSHandler::registry().getComponentsArray<OutlineComponent, TransformComponent, ModelComponent, ComponentsModule::AABBComponent>()) {
			if (!&modelComp || !&transform || !&aabbcomp) {
				continue;
			}
			if (aabbcomp.aabbs.empty()) {
				continue;
			}

			auto& model = modelComp.getModel();
			int i = 0;
			for (auto& mesh : model.mMeshHandles) {
				aabbcomp.mtx.lock_shared();
				if (aabbcomp.aabbs[i].isOnFrustum(renderDataHandle.mCamFrustum)) {
					batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform.getTransform(), false);
				}
				aabbcomp.mtx.unlock_shared();
				i++;
			}
		}
		batcher.sort(ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos());

		batcher.flushAll(true);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);

		auto outlineG = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_outline.vs", "shaders/g_outline.fs");
		outlineG->use();
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE26, GL_TEXTURE_2D, mData.gNormal);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE27, GL_TEXTURE_2D, mData.gOutlines);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE25, GL_TEXTURE_2D, mData.gLights);
		outlineG->setInt("gDepth", 26);
		outlineG->setInt("gOutlinesP", 27);
		outlineG->setInt("gLightsP", 25);

		Utils::renderQuad();
	}
	else {
		if (needClearOutlines) {
			needClearOutlines = false;
			glBindFramebuffer(GL_FRAMEBUFFER, mOData.mFramebuffer);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (renderDataHandle.mRenderType == SystemsModule::RenderMode::WIREFRAME) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	
}
