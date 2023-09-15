#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "ecsModule/ECSHandler.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/CascadeShadows.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "systemsModule/RenderSystem.h"
#include "core/BoundingVolume.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"

#include <thread>

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "core/FileSystem.h"
#include "propertiesModule/PropertiesSystem.h"

using namespace Engine::RenderModule::RenderPasses;

CascadedShadowPass::CascadedShadowPass() : mShadowSource(nullptr) {
}

CascadedShadowPass::~CascadedShadowPass() {
	freeBuffers();
}

void CascadedShadowPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = ecsModule::ECSHandler::entityManagerInstance()->createEntity<CascadeShadows>(glm::vec2{4096.f, 4096.f});
	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);


	mShadowSource->getComponent<TransformComponent>()->setRotate({ -0.4f * 180.f,0.f, 0.4f * 5.f });
	mShadowSource->getComponent<TransformComponent>()->reloadTransform();

	initRender();
}

void CascadedShadowPass::initRender() {
	freeBuffers();

	auto cmp = mShadowSource->getComponent<CascadeShadowComponent>();
	auto& cameraProjection = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<CameraComponent>()->getProjection();

	cmp->updateCascades(cameraProjection);

	glGenFramebuffers(1, &lightFBO);

	glGenTextures(1, &lightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	glTexImage3D(
		GL_TEXTURE_2D_ARRAY,
		0,
		GL_DEPTH_COMPONENT32,
		static_cast<int>(cmp->resolution.x),
		static_cast<int>(cmp->resolution.y),
		static_cast<int>(cmp->cascades.size()),
		0,
		GL_DEPTH_COMPONENT,
		GL_FLOAT,
		nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		Engine::LogsModule::Logger::LOG_ERROR("FRAMEBUFFER::CascadeShadow Framebuffer is not complete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenBuffers(1, &matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void CascadedShadowPass::freeBuffers() const {
	glDeleteFramebuffers(1, &lightFBO);
	glDeleteTextures(1, &lightDepthMaps);
	glDeleteBuffers(1, &matricesUBO);
}

void CascadedShadowPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	if (!mInited) {
		return;
	}

	if (!renderer) {
		return;
	}

	if (ImGui::Begin("lightSpaceMatrix")) {
		ImGui::DragFloat("camera speed", &ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->MovementSpeed, 0.1f);
		ImGui::DragFloat("shadows update delta", &mUpdateDelta, 0.1f);

		if (mShadowSource) {
			if (ImGui::Button("cache")) {
				mShadowSource->cacheMatrices();
			}

			if (ImGui::Button("clear")) {
				mShadowSource->clearCacheMatrices();
			}
			static float sunProgress = 0.4f;
			if (ImGui::DragFloat("sun pos", &sunProgress, 0.001f, 0.f)) {
				auto x = glm::cos(glm::radians(-sunProgress * 180.f));
				auto y = glm::sin(glm::radians(sunProgress * 180.f));
				auto z = glm::sin(glm::radians(sunProgress * 180.f));
				mShadowSource->getComponent<TransformComponent>()->setRotate({ -sunProgress * 180.f,0.f, sunProgress * 5.f });
				mShadowSource->getComponent<TransformComponent>()->reloadTransform();
			}
		}
	}
	ImGui::End();

	if (mUpdateTimer <= mUpdateDelta) {
		mUpdateTimer += UnnamedEngine::instance()->getDeltaTime();

		renderDataHandle.mCascadedShadowsPassData = {
			lightDepthMaps,
			mShadowSource->getComponent<TransformComponent>()->getForward(),
			mShadowSource->getComponent<LightSourceComponent>()->getLightColor(),
			mShadowSource->getComponent<CascadeShadowComponent>()->resolution,
			mShadowSource->getComponent<CascadeShadowComponent>()->cascades.back().viewProjection.getFar(),
			mShadowSource->getComponent<CascadeShadowComponent>()->shadowCascadeLevels,
			mShadowSource,
			mShadowSource->getComponent<CascadeShadowComponent>()->cascades,
			mShadowSource->getComponent<CascadeShadowComponent>()->shadowIntensity
		};

		return;
	}

	mUpdateTimer = 0.f;


	auto batcher = renderer->getBatcher();

	const auto& drawableEntities = renderDataHandle.mDrawableEntities;

	auto addToDraw = [batcher, &drawableEntities, this](size_t chunkBegin, size_t chunkEnd) {
		for (size_t i = chunkBegin; i < chunkEnd; i++) {
			auto entityId = drawableEntities[i];
			auto transform = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
			auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<ModelComponent>();
			if (transform && modelComp) {
				const auto& transformMatrix = transform->getTransform();
				for (auto& mesh : modelComp->getModelLowestDetails().mMeshHandles) {
					auto it = std::ranges::find_if(std::as_const(mShadowSource->getComponent<CascadeShadowComponent>()->cascades), [&mesh, &transformMatrix](const ComponentsModule::ShadowCascade& shadow) {
						return mesh.mBounds->isOnFrustum(shadow.frustum, transformMatrix);
					});

					if (it != mShadowSource->getComponent<CascadeShadowComponent>()->cascades.cend()) {

						mtx.lock();
						batcher->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transformMatrix, false);
						mtx.unlock();
					}
				}
			}
		}
	};

	auto size = drawableEntities.size();
	size_t chunk = 0;
	size_t growSpeed = 500;

	threads.reserve(size / growSpeed);

	while (chunk < size) {
		threads.emplace_back(addToDraw, chunk, std::min(chunk + growSpeed, size));
		chunk += growSpeed;
	}

	auto cmp = mShadowSource->getComponent<CascadeShadowComponent>();

	const auto& lightMatrices = mShadowSource->getLightSpaceMatrices();
	if (!lightMatrices.empty()) {
		glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4x4) * lightMatrices.size(), &lightMatrices[0]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, lightDepthMaps, 0);
	glViewport(0, 0, static_cast<int>(cmp->resolution.x), static_cast<int>(cmp->resolution.y));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();

	for (auto& thread : threads) {
		thread.join();
	}
	threads.clear();

	//glEnable(GL_DEPTH_CLAMP);
	renderer->getBatcher()->flushAll(true, mShadowSource->getComponent<TransformComponent>()->getPos(true));
	//glDisable(GL_DEPTH_CLAMP);
	//draw meshes which should cast shadow


	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Engine::RenderModule::Renderer::SCR_WIDTH, Engine::RenderModule::Renderer::SCR_HEIGHT);

	renderDataHandle.mCascadedShadowsPassData = {
			lightDepthMaps,
			mShadowSource->getComponent<TransformComponent>()->getForward(),
			mShadowSource->getComponent<LightSourceComponent>()->getLightColor(),
			mShadowSource->getComponent<CascadeShadowComponent>()->resolution,
			mShadowSource->getComponent<CascadeShadowComponent>()->cascades.back().viewProjection.getFar(),
			mShadowSource->getComponent<CascadeShadowComponent>()->shadowCascadeLevels,
			mShadowSource,
			mShadowSource->getComponent<CascadeShadowComponent>()->cascades,
			mShadowSource->getComponent<CascadeShadowComponent>()->shadowIntensity
	};
}
