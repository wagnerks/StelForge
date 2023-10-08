#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "renderModule/CascadeShadows.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "systemsModule/RenderSystem.h"
#include "core/BoundingVolume.h"
#include "systemsModule/CameraSystem.h"

#include <thread>

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "core/ECSHandler.h"
#include "core/FileSystem.h"
#include "core/ThreadPool.h"
#include "..\..\ecss\Registry.h"
#include "logsModule/logger.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/SystemManager.h"

using namespace Engine::RenderModule::RenderPasses;

CascadedShadowPass::CascadedShadowPass() {
}

CascadedShadowPass::~CascadedShadowPass() {
	freeBuffers();
}

void CascadedShadowPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = ECSHandler::registry()->takeEntity();
	
	ECSHandler::registry()->addComponent<TransformComponent>(mShadowSource);
	ECSHandler::registry()->addComponent<LightSourceComponent>(mShadowSource, Engine::ComponentsModule::eLightType::WORLD);

	auto cmp = ECSHandler::registry()->addComponent<CascadeShadowComponent>(mShadowSource);
	cmp->resolution = glm::vec2{ 4096.f, 4096.f };

	auto debugData = ECSHandler::registry()->addComponent<DebugDataComponent>(mShadowSource);
	debugData->stringId = "cascadeShadows";

	auto cam = ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto& cameraProjection = ECSHandler::registry()->getComponent<CameraComponent>(cam)->getProjection();

	cmp->shadowCascadeLevels = { cameraProjection.getNear(), 50.f, 150.f, 500.f, cameraProjection.getFar() };

	auto shadow = FileSystem::readJson("cascadedShadows.json");

	PropertiesModule::PropertiesSystem::deserializeProperty<CascadeShadowComponent>(mShadowSource, shadow["Properties"]);


	ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->setRotate({ -0.4f * 180.f,0.f, 0.4f * 5.f });
	ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->reloadTransform();

	initRender();
}

void CascadedShadowPass::initRender() {
	freeBuffers();

	auto cmp = ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource);
	auto& cameraProjection = ECSHandler::registry()->getComponent<CameraComponent>(ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection();

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

	glGenBuffers(1, &matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 6, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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


	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			if (ImGui::BeginMenu("Shadows debug")) {
				ImGui::DragFloat("shadows update delta", &mUpdateDelta, 0.1f);

				if (mShadowSource) {
					if (ImGui::Button("cache")) {
						ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->cacheMatrices();
					}

					if (ImGui::Button("clear")) {
						ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->clearCacheMatrices();
					}
					static float sunProgress = 0.4f;
					if (ImGui::DragFloat("sun pos", &sunProgress, 0.001f, 0.f)) {
						auto x = glm::cos(glm::radians(-sunProgress * 180.f));
						auto y = glm::sin(glm::radians(sunProgress * 180.f));
						auto z = glm::sin(glm::radians(sunProgress * 180.f));
						ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->setRotate({ -sunProgress * 180.f,0.f, sunProgress * 5.f });
						ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->reloadTransform();
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
	}
	ImGui::EndMainMenuBar();

	if (mUpdateTimer <= mUpdateDelta) {
		mUpdateTimer += UnnamedEngine::instance()->getDeltaTime();

		renderDataHandle.mCascadedShadowsPassData = {
			lightDepthMaps,
			ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->getForward(),
			ECSHandler::registry()->getComponent<LightSourceComponent>(mShadowSource)->getLightColor(),
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->resolution,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->cascades.back().viewProjection.getFar(),
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->shadowCascadeLevels,
			mShadowSource,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->cascades,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->shadowIntensity
		};

		return;
	}

	mUpdateTimer = 0.f;


	auto batcher = renderer->getBatcher();


	auto future = ThreadPool::instance()->addRenderTask([&](std::mutex& mtx) {
		const auto shadowsComp = ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource);
		if (shadowsComp) {
			for (auto [isDraw, modelComp, transform] : ECSHandler::registry()->getComponentsArray<const IsDrawableComponent, const ModelComponent, const TransformComponent>()) {
				if (!&isDraw) {
					continue;
				}
				if (!&modelComp) {
					continue;
				}

				const auto& transformMatrix = transform.getTransform();
				for (auto& mesh : modelComp.getModelLowestDetails().mMeshHandles) {
					auto it = std::ranges::find_if(std::as_const(shadowsComp->cascades), [&mesh, &transformMatrix](const ComponentsModule::ShadowCascade& shadow) {
						return mesh.mBounds && mesh.mBounds->isOnFrustum(shadow.frustum, transformMatrix);
					});
					if (it != shadowsComp->cascades.cend()) {
						batcher->addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transformMatrix, false);
					}
				}
			}
		}
	});
	

	auto cmp = ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource);

	const auto& lightMatrices = ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->getLightSpaceMatrices();
	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);

	if (!lightMatrices.empty()) {
		glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4x4) * lightMatrices.size(), &lightMatrices[0]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, lightDepthMaps, 0);
	glViewport(0, 0, static_cast<int>(cmp->resolution.x), static_cast<int>(cmp->resolution.y));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/cascadeShadowMap.gs");
	simpleDepthShader->use();

	
	future.get();
	

	renderer->getBatcher()->flushAll(true, ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->getPos(true));


	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Engine::RenderModule::Renderer::SCR_WIDTH, Engine::RenderModule::Renderer::SCR_HEIGHT);

	renderDataHandle.mCascadedShadowsPassData = {
			lightDepthMaps,
			ECSHandler::registry()->getComponent<TransformComponent>(mShadowSource)->getForward(),
			ECSHandler::registry()->getComponent<LightSourceComponent>(mShadowSource)->getLightColor(),
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->resolution,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->cascades.back().viewProjection.getFar(),
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->shadowCascadeLevels,
			mShadowSource,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->cascades,
			ECSHandler::registry()->getComponent<CascadeShadowComponent>(mShadowSource)->shadowIntensity
	};
}
