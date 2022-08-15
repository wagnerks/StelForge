#include "CascadedShadowPass.h"

#include "imgui.h"
#include "componentsModule/LodComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/ECSHandler.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/CascadeShadows.h"
#include "renderModule/Renderer.h"
#include "renderModule/Utils.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/RenderSystem.h"

using namespace GameEngine::RenderModule::RenderPasses;

CascadedShadowPass::CascadedShadowPass() : mShadowSource(nullptr) {
}

void CascadedShadowPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	mShadowSource = new CascadeShadows({1024.f,1024.f});
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

	auto lightSpaceMatrices = mShadowSource->getLightSpaceMatrices();
	

	//cull meshes
	for (auto entityId : drawableEntities) {
		auto transform = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
		if (auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<ModelComponent>()){
			if (auto model = modelComp->getModel()) {
				size_t LODLevel = 0;
				if (auto lodComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<LodComponent>()) {
					LODLevel = lodComp->getLodLevel();
				}
				for (auto& mesh : modelComp->getModel()->getMeshes(LODLevel)) {
					bool pass = false;
					for (auto i = 0; i < lightSpaceMatrices.size(); i++) {
						auto modelT = lightSpaceMatrices[i] * transform->getTransform();
						auto pos = glm::vec3(modelT[3]);
						auto radius = -0.3f;
						auto k = 1.f;
						if (pos.x - radius > -k && pos.x + radius < k) {
							if (pos.y - radius > -k && pos.y + radius < k) {
								if (pos.z - radius > -k && pos.z + radius < k) {
									pass = true;
								}
							}
						}
						if (pass) {
							break;
						}
					}
					pass = true;
					if (pass) {
						renderer->getBatcher()->addToDrawList(mesh->getVAO(), mesh->vertices.size(), mesh->indices.size(),model->getTextures(), transform->getTransform(), false);
					}
				}
			}
		}
		else {
			renderer->getBatcher()->addToDrawList(Utils::cubeVAO, 36, 0,{}, transform->getTransform(), false);
		}
	}

	renderer->getBatcher()->flushAll(true, mShadowSource->getLightPosition(), true);
	
	//draw meshes which should cast shadow

	mShadowSource->postDraw();

	mData = {
		mShadowSource->getBias(),
		mShadowSource->getShadowMapTextureArray(),
		mShadowSource->getLightDirection(),
		mShadowSource->getLightColor(),
		mShadowSource->getResolution(),
		mShadowSource->getLightPosition(),
		mShadowSource->getCameraFarPlane(),
		mShadowSource->getShadowCascadeLevels(),
		mShadowSource
	};

	renderDataHandle.mCascadedShadowsPassData = mData;

	if (ImGui::Begin("lightSpaceMatrix")) {
		ImGui::DragFloat("camera speed", &Engine::getInstance()->getCamera()->MovementSpeed, 0.1f);

		if (mShadowSource) {
			if(ImGui::Button("cache")) {
				mShadowSource->cacheMatrices();
			}

			if(ImGui::Button("clear")) {
				mShadowSource->clearCacheMatrices();
			}

			float color[3] = {mShadowSource->getLightColor().x, mShadowSource->getLightColor().y, mShadowSource->getLightColor().z};
			if (ImGui::ColorEdit3("sunColor", color)) {
				mShadowSource->setLightColor({color[0], color[1], color[2]});
			}
			float lightPosDir[] = {mShadowSource->getLightPosition().x, mShadowSource->getLightPosition().y, mShadowSource->getLightPosition().z};
			if (ImGui::DragFloat3("sun position",lightPosDir , 0.01f)) {
				mShadowSource->setLightPosition({lightPosDir[0], lightPosDir[1], lightPosDir[2]});
			}
			float cascadeBias = mShadowSource->getBias();
			if (ImGui::DragFloat("bias", &cascadeBias, 0.00001f,0.f,1.f, "%.5f")) {
				mShadowSource->setBias(cascadeBias);
			}

			if (ImGui::DragFloat("sun pos", &mShadowSource->sunProgress, 0.001f, 0.f)) {
				auto x = glm::cos(glm::radians(-mShadowSource->sunProgress * 180.f));
				auto y = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
				auto z = glm::sin(glm::radians(mShadowSource->sunProgress * 180.f));
				mShadowSource->setLightPosition({x * 80.f, y * 30.f, z * 10.f + 0.001f});
			}
		}
	}
	ImGui::End();
}
