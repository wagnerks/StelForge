#include "PointLightPass.h"

#include <shared_mutex>

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"

#include "logsModule/logger.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"

namespace SFE::Render::RenderPasses {
	void PointLightPass::init() {
		lightProjection = SFE::ProjectionModule::PerspectiveProjection(90.f, 1.f, 0.01f, 100);
		freeBuffers();

		mLightDepthMaps.image3D(shadowResolution, shadowResolution, maxShadowFaces, AssetsModule::DEPTH_COMPONENT32, AssetsModule::DEPTH_COMPONENT, AssetsModule::FLOAT);
		mLightDepthMaps.setParameter({
			{AssetsModule::TEXTURE_MIN_FILTER, GL_LINEAR},
			{AssetsModule::TEXTURE_MAG_FILTER, GL_LINEAR},
			{AssetsModule::TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE},
			{AssetsModule::TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE},
			{AssetsModule::TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE},
			{AssetsModule::TEXTURE_COMPARE_FUNC, GL_LESS},
		});

		lightFramebuffer.bind();
		lightFramebuffer.addAttachmentTexture(GL_DEPTH_ATTACHMENT, &mLightDepthMaps);
		lightFramebuffer.setDrawBuffer(NONE);
		lightFramebuffer.setReadBuffer(NONE);
		lightFramebuffer.finalize();

		auto guard = mMatricesUBO.bindWithGuard();
		mMatricesUBO.allocateData<Math::Mat4>(maxShadowFaces, STATIC_DRAW);
		mMatricesUBO.setBufferBinding(lightMatricesBinding);

		Framebuffer::bindDefaultFramebuffer();
	}

	void PointLightPass::freeBuffers() const {
	}

	void PointLightPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
		if (!renderer) {
			return;
		}

		FUNCTION_BENCHMARK
		lightMatrices.clear();
		frustums.clear();

		renderDataHandle.mPointPassData.shadowEntities.clear();

		offsets.clear();
		
		for (const auto& [entity,lightSource, transform] : ECSHandler::registry().getComponentsArray<LightSourceComponent, TransformComponent>()) {
			//todo check is light side frustum in camera frustum, and filter it to ignore light sources which is not on your screen
			if (!FrustumModule::SquareAABB::isOnFrustum(renderDataHandle.mCamFrustum, transform->getPos(true), lightSource->mRadius)) {
				continue;
			}

			//todo some dirty logic
			switch (lightSource->getType()) {
			case ComponentsModule::eLightType::DIRECTIONAL: {
				/*float shadowHeight = 100.f;
				float shadowWidth = 100.f;
				SFE::ProjectionModule::OrthoProjection proj = SFE::ProjectionModule::OrthoProjection({ -shadowWidth * 0.5f, -shadowHeight * 0.5f }, { shadowWidth * 0.5f, shadowHeight * 0.5f }, 0.01f, lightSource.mRadius);

				lightMatrices.push_back(proj.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));*/
				break;
			}
			case ComponentsModule::eLightType::POINT: {
				renderDataHandle.mPointPassData.shadowEntities.push_back(entity);
				offsets.emplace_back(entity, lightSource->getTypeOffset(lightSource->getType()));
				
				fillMatrix(transform->getPos(true), lightSource->mNear, lightSource->mRadius);
				break;
			}
			case ComponentsModule::eLightType::PERSPECTIVE: {
				//lightProjection.setFar(lightSource.mRadius);//here possible some fov settings
				//lightMatrices.push_back(lightProjection.getProjectionsMatrix() * owner->getComponent<TransformComponent>()->getViewMatrix());
				//frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
				break;
			}
			default:;
			}
		}

		if (offsets.empty()) {
			return;
		}

		if (!lightMatrices.empty()) {
			auto guard = mMatricesUBO.bindWithGuard();
			mMatricesUBO.setData(lightMatrices.size(), lightMatrices.data());
		}

		AssetsModule::TextureHandler::instance()->bindTextureToSlot(30, &mLightDepthMaps);
		lightFramebuffer.bind();
		glViewport(0, 0, shadowResolution, shadowResolution);
		glClear(GL_DEPTH_BUFFER_BIT);


		int offsetSum = 0;
		for (auto& offset : offsets) {
			auto simpleDepthShader = SHADER_CONTROLLER->loadGeometryShader("shaders/cascadeShadowMap.vs", "shaders/cascadeShadowMap.fs", "shaders/pointLightMap.gs");
			simpleDepthShader->use();
			simpleDepthShader->setInt("offset", offsetSum);

			auto ocTreeSystem = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			ocTreeSystem->readLock();

			std::vector<ecss::SectorId> entities;
 
			for (auto& [coords, tree] : ocTreeSystem->mOctrees) {
				auto lock = tree.readLock();
				tree.forEachObject([this, &entities, offsetSum, &offset](const auto& obj) {
					for (auto i = offsetSum; i < offset.second + offsetSum; i++) {
						if (FrustumModule::AABB::isOnFrustum(frustums[i], obj.pos + SFE::Math::Vec3(obj.size.x, -obj.size.y, obj.size.z) * 0.5f, obj.size)){
							entities.emplace_back(obj.data.getID());
							break;
						}
					}
						
					
					
				}, [this, offsetSum, &offset](const SFE::Math::Vec3& pos, float size, auto&) {
					for (auto i = offsetSum; i < offset.second + offsetSum; i++) {
						if (OcTree<ecss::EntityHandle>::isOnFrustum(frustums[i], pos, size)) {
							return true;
						}
					}
					return false;
				});
			}

			std::sort(entities.begin(), entities.end());
			for (const auto& [entity, mod, draw, trans  ] : ECSHandler::registry().getComponentsArray<const ModelComponent, const IsDrawableComponent, const TransformComponent>(entities)) {
				if (!trans || !mod || !draw) {
					continue;
				}

				const auto& transformMatrix = trans->getTransform();
				for (auto& mesh : mod->getModelLowestDetails().meshes) {
					batcher.addToDrawList(mesh->getVAO(), mesh->mData.vertices.size(), mesh->mData.indices.size(), mesh->mMaterial, transformMatrix, {}, false);
				}
			}
			batcher.sort(ECSHandler::registry().getComponent<TransformComponent>(offset.first)->getPos(true));

			offsetSum += offset.second;
			
			batcher.flushAll(true);
		}

		Framebuffer::bindDefaultFramebuffer();

		glViewport(0, 0, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);
	}

	void PointLightPass::fillMatrix(Math::Vec3 globalLightPos, float lightNear, float lightRadius) {
		lightProjection.setNearFar(lightNear, lightRadius);

		const auto transform = Math::translate(Math::Mat4(1.0f), globalLightPos);

		for (auto angle : { 0.f, 90.f, -90.f, 180.f }) {
			//todo rotate matrix instead creating rotated copy
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * Math::inverse(Math::rotate(transform, Math::radians(angle), { 0.f,1.f,0.f })));
			frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
		}

		for (auto angle : { 90.f, -90.f }) {
			auto res = Math::rotate(transform, Math::radians(angle), { 1.f,0.f,0.f });
			lightMatrices.push_back(lightProjection.getProjectionsMatrix() * Math::inverse(res));
			frustums.push_back(SFE::FrustumModule::createFrustum(lightMatrices.back()));
		}
	}
}
