#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LodComponent.h"
#include "componentsModule/MeshComponent.h"
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
}

void CascadedShadowPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	if (!mInited) {
		return;
	}

	if (!renderer) {
		return;
	}

	auto& drawableEntities = renderDataHandle.mDrawableEntities;

	mShadowSource->preDraw();

	//cull meshes
	for (auto entityId : drawableEntities) {
		auto transform = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
		if (auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<MeshComponent>()) {

			size_t LODLevel = 0;
			if (auto lodComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<LodComponent>()) {
				LODLevel = lodComp->getLodLevel();
			}

			auto& mesh = modelComp->getMesh(LODLevel);
			bool pass = false;
			for (auto shadow : mShadowSource->shadows) {
				if (mesh.mBounds->isOnFrustum(*shadow->getComponent<FrustumComponent>()->getFrustum(), *transform)) {
					pass = true;
					break;
				}
			}

			if (pass) {
				renderer->getBatcher()->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transform->getTransform(), false);
			}
		}
	}

	renderer->getBatcher()->flushAll(true, mShadowSource->getLightPosition(), true);

	//draw meshes which should cast shadow

	mShadowSource->postDraw();

	mData = {
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

	renderDataHandle.mCascadedShadowsPassData = mData;

	if (ImGui::Begin("lightSpaceMatrix")) {
		ImGui::DragFloat("camera speed", &ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->MovementSpeed, 0.1f);

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
}
