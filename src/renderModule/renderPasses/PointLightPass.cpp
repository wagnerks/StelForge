#include "PointLightPass.h"

#include <mat4x4.hpp>

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"

#include "logsModule/logger.h"
#include "systemsModule/CameraSystem.h"
#include "systemsModule/RenderSystem.h"

namespace Engine::RenderModule::RenderPasses {
	void PointLightPass::initRender() {
		lightProjection = Engine::ProjectionModule::PerspectiveProjection(90.f, 1.f, 0.01f, 100);
		freeBuffers();

		glGenFramebuffers(1, &mFramebufferID);

		glGenTextures(1, &mLightDepthMaps);
		glBindTexture(GL_TEXTURE_2D_ARRAY, mLightDepthMaps);
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_DEPTH_COMPONENT32,
			static_cast<int>(shadowResolution),//x size
			static_cast<int>(shadowResolution),//y size
			maxShadowFaces,//all cube faces
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

		glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mLightDepthMaps, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			Engine::LogsModule::Logger::LOG_ERROR("FRAMEBUFFER::CascadeShadow Framebuffer is not complete!");
		}

		glGenBuffers(1, &mMatricesUBO);

		glBindBuffer(GL_UNIFORM_BUFFER, mMatricesUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * maxShadowFaces, nullptr, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, lightMatricesBinding, mMatricesUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);


		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PointLightPass::freeBuffers() const {
		glDeleteFramebuffers(1, &mFramebufferID);
		glDeleteTextures(1, &mLightDepthMaps);
		glDeleteBuffers(1, &mMatricesUBO);
	}

	void PointLightPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
		if (!renderer) {
			return;
		}

		lightMatrices.clear();
		frustums.clear();

		renderDataHandle.mPointPassData.shadowEntities.clear();

		offsets.clear();
		auto& lightSources = *ECSHandler::componentManagerInstance()->getComponentContainer<LightSourceComponent>();
		for (auto& lightSource : lightSources) {
			//todo some dirty logic
			switch (lightSource.getType()) {
			case ComponentsModule::eLightType::DIRECTIONAL: {
				/*float shadowHeight = 100.f;
				float shadowWidth = 100.f;
				Engine::ProjectionModule::OrthoProjection proj = Engine::ProjectionModule::OrthoProjection({ -shadowWidth * 0.5f, -shadowHeight * 0.5f }, { shadowWidth * 0.5f, shadowHeight * 0.5f }, 0.01f, lightSource.mRadius);

				lightMatrices.push_back(proj.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				frustums.push_back(Engine::FrustumModule::createFrustum(lightMatrices.back()));*/
				break;
			}
			case ComponentsModule::eLightType::POINT: {
				auto owner = ECSHandler::entityManagerInstance()->getEntity(lightSource.getOwnerId());
				renderDataHandle.mPointPassData.shadowEntities.push_back(owner->getEntityID());
				offsets.emplace_back(owner->getEntityID(), lightSource.getTypeOffset(lightSource.getType()));
				fillMatrix(owner->getComponent<TransformComponent>()->getPos(true), lightSource.mRadius);
				break;
			}
			case ComponentsModule::eLightType::PERSPECTIVE: {
				//lightProjection.setFar(lightSource.mRadius);//here possible some fov settings
				//lightMatrices.push_back(lightProjection.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				//frustums.push_back(Engine::FrustumModule::createFrustum(lightMatrices.back()));
				break;
			}
			default:;
			}
		}

		if (!lightMatrices.empty()) {
			glBindBuffer(GL_UNIFORM_BUFFER, mMatricesUBO);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4x4) * lightMatrices.size(), &lightMatrices[0]);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE30, GL_TEXTURE_2D_ARRAY, mLightDepthMaps);

		glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, mLightDepthMaps, 0);
		glViewport(0, 0, static_cast<int>(shadowResolution), static_cast<int>(shadowResolution));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		auto batcher = renderer->getBatcher();

		int offsetSum = 0;
		for (auto& offset : offsets) {
			auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/pointLightMap.gs");
			simpleDepthShader->use();
			simpleDepthShader->setInt("offset", offsetSum);

			for (auto entityId : renderDataHandle.mDrawableEntities) {
				auto transform = ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
				auto modelComp = ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<ModelComponent>();
				if (transform && modelComp) {
					const auto& transformMatrix = transform->getTransform();
					for (auto& mesh : modelComp->getModelLowestDetails().mMeshHandles) {
						for (auto i = offsetSum; i < offset.second + offsetSum; i++) {
							if (mesh.mBounds->isOnFrustum(frustums[i], transformMatrix)) {
								batcher->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transformMatrix, false);
								break;
							}
						}

					}
				}
			}

			offsetSum += offset.second;

			renderer->getBatcher()->flushAll(true, ECSHandler::entityManagerInstance()->getEntity(offset.first)->getComponent<TransformComponent>()->getPos(true));
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, Engine::RenderModule::Renderer::SCR_WIDTH, Engine::RenderModule::Renderer::SCR_HEIGHT);
	}

	void PointLightPass::fillMatrix(glm::vec3 globalLightPos, float lightRadius) {
		lightProjection.setFar(lightRadius);

		const auto transform = glm::translate(glm::mat4(1.0f), globalLightPos);

		for (auto angle : { 0.f, 90.f, -90.f, 180.f }) {
			auto res = glm::rotate(transform, glm::radians(angle), { 0.f,1.f,0.f });//todo rotate matrix instead creating rotated copy
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * glm::inverse(res));
			frustums.push_back(Engine::FrustumModule::createFrustum(lightMatrices.back()));
		}

		for (auto angle : { 90.f, -90.f }) {
			auto res = glm::rotate(transform, glm::radians(angle), { 1.f,0.f,0.f });
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * glm::inverse(res));
			frustums.push_back(Engine::FrustumModule::createFrustum(lightMatrices.back()));
		}
	}
}
