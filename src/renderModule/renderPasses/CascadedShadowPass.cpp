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

using namespace Engine::RenderModule::RenderPasses;

CascadedShadowPass::CascadedShadowPass() : mShadowSource(nullptr) {
}

void CascadedShadowPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = ecsModule::ECSHandler::entityManagerInstance()->createEntity<CascadeShadows>(glm::vec2{4096.f, 4096.f});
	mShadowSource->init();

	mShadowSource->getComponent<TransformComponent>()->setRotate({ -mShadowSource->sunProgress * 180.f,0.f, mShadowSource->sunProgress * 5.f });
	mShadowSource->getComponent<TransformComponent>()->reloadTransform();
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

			if (ImGui::DragFloat("sun pos", &mShadowSource->sunProgress, 0.001f, 0.f)) {
				auto x = glm::cos(glm::radians(-mShadowSource->sunProgress * 180.f));
				auto y = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
				auto z = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
				mShadowSource->getComponent<TransformComponent>()->setRotate({ -mShadowSource->sunProgress * 180.f,0.f, mShadowSource->sunProgress * 5.f });
				mShadowSource->getComponent<TransformComponent>()->reloadTransform();
			}

			/*mShadowSource->sunProgress += 0.00001f;
			auto x = glm::cos(glm::radians(-mShadowSource->sunProgress * 180.f));
			auto y = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
			auto z = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
			mShadowSource->setLightPosition({x * 80.f, y * 30.f, z * 10.f + 0.001f});*/
		}
	}
	ImGui::End();

	if (mUpdateTimer <= mUpdateDelta) {
		mUpdateTimer += UnnamedEngine::instance()->getDeltaTime();

		renderDataHandle.mCascadedShadowsPassData = {
			mShadowSource->getShadowMapTextureArray(),
			mShadowSource->getLightDirection(),
			glm::vec3{1.f},
			mShadowSource->getResolution(),
			mShadowSource->getLightPosition(),
			mShadowSource->getCameraFarPlane(),
			mShadowSource->getShadowCascadeLevels(),
			mShadowSource,
			mShadowSource->shadows
		};

		return;
	}

	mUpdateTimer = 0.f;

	auto& drawableEntities = renderDataHandle.mDrawableEntities;

	mShadowSource->preDraw();

	//cull meshes
	for (auto entityId : drawableEntities) {
		auto transform = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
		auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<ModelComponent>();
		if (transform && modelComp) {
			for (auto& mesh : modelComp->getModelLowestDetails().mMeshHandles) {
				auto it = std::ranges::find_if(std::as_const(mShadowSource->shadows), [&mesh, &transform](const CascadeShadow* shadow) {
					return mesh.mBounds->isOnFrustum(*shadow->getComponent<FrustumComponent>()->getFrustum(), *transform);
				});

				if (it != mShadowSource->shadows.cend()) {
					renderer->getBatcher()->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transform->getTransform(), false);
				}
			}
		}
	}

	renderer->getBatcher()->flushAll(true, mShadowSource->getLightPosition(), true);

	//draw meshes which should cast shadow

	mShadowSource->postDraw();

	renderDataHandle.mCascadedShadowsPassData = {
			mShadowSource->getShadowMapTextureArray(),
			mShadowSource->getLightDirection(),
			glm::vec3{1.f},
			mShadowSource->getResolution(),
			mShadowSource->getLightPosition(),
			mShadowSource->getCameraFarPlane(),
			mShadowSource->getShadowCascadeLevels(),
			mShadowSource,
			mShadowSource->shadows
	};
}
