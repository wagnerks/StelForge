#include "RenderSystem.h"
#include "renderModule/Utils.h"

#include <algorithm>

#include "CameraSystem.h"
#include "imgui.h"
#include "OcTreeSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "renderModule/renderPasses/DebugPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/ShadersPass.h"

#include "componentsModule/TransformComponent.h"
#include "renderModule/TextRenderer.h"
#include "renderModule/renderPasses/GUIPass.h"
#include "renderModule/renderPasses/OcclusionPass.h"

namespace SFE::SystemsModule {

	template <typename PassType>
	void RenderSystem::addRenderPass() {
		size_t i = 0;
		for (; i < RENDER_PASSES_PRIORITY.size(); i++) {
			if (RENDER_PASSES_PRIORITY[i] == typeid(PassType).hash_code()) {
				break;
			}
		}

		assert(i != RENDER_PASSES_PRIORITY.size());
		if (i == RENDER_PASSES_PRIORITY.size()) {
			return;
		}

		auto pass = new PassType();
		pass->init();
		pass->setPriority(i);

		mRenderPasses.emplace_back(pass);

		std::sort(mRenderPasses.begin(), mRenderPasses.end(), [](Render::RenderPass* a, Render::RenderPass* b) {
			return a->getPriority() < b->getPriority();
		});
	}

	RenderSystem::RenderSystem() : System({ SFE::SystemsModule::TaskType::TRAHSFORM_RELOADED , SFE::SystemsModule::TaskType::ARMATURE_UPDATED, MATERIAL_UPDATED, MESH_UPDATED }) {
		mRenderPasses.reserve(RENDER_PASSES_PRIORITY.size());

		addRenderPass<Render::RenderPasses::OcclusionPass>();
		addRenderPass<Render::RenderPasses::CascadedShadowPass>();//todo passes shoudle be created according to settings
		addRenderPass<Render::RenderPasses::PointLightPass>();
		addRenderPass<Render::RenderPasses::GeometryPass>();
		addRenderPass<Render::RenderPasses::ShadersPass>();
		addRenderPass<Render::RenderPasses::LightingPass>();
		addRenderPass<Render::RenderPasses::SSAOPass>();
		addRenderPass<Render::RenderPasses::DebugPass>();
		addRenderPass<Render::RenderPasses::GUIPass>();
	
		auto guard = cameraMatricesUBO.lock();
		cameraMatricesUBO.allocateData<RenderMatrices>(1, GLW::DYNAMIC_DRAW);
		cameraMatricesUBO.setBufferBinding(5);

		ECSHandler::drawRegistry(0).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);
		ECSHandler::drawRegistry(1).registerArray<ComponentsModule::TransformMatComp, MeshComponent, MaterialComponent, ComponentsModule::ArmatureBonesComponent>(0);
	}

	void RenderSystem::update(float_t dt) {
		FUNCTION_BENCHMARK;

		mRenderData.current = mRenderData.next;
		mRenderData.cameraProjection = mRenderData.nextCameraProjection;

		mRenderData.mCameraPos = mRenderData.mNextCameraPos;
		mRenderData.mViewDir = mRenderData.mNextViewDir;
		mRenderData.mCamFrustum = mRenderData.mNextCamFrustum;

		{
			auto lock = cameraMatricesUBO.lock();
			cameraMatricesUBO.setData(1, &mRenderData.current);
		}

		auto playerCamera = ECSHandler::getSystem<CameraSystem>()->getCurrentCamera();
		const auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(playerCamera);
		const auto transformComp = ECSHandler::registry().pinComponent<const TransformComponent>(playerCamera);

		mRenderData.nextCameraProjection = cameraComp->getProjection();
		mRenderData.next.projection = mRenderData.nextCameraProjection.getProjectionsMatrix();
		mRenderData.next.view = transformComp->getViewMatrix();
		mRenderData.next.PV = mRenderData.next.projection * mRenderData.next.view;

		mRenderData.mNextCameraPos = transformComp->getPos(true);
		mRenderData.mNextViewDir = normalize(transformComp->getForward());
		mRenderData.mNextCamFrustum = cameraComp->getFrustum();

		mRenderData.rotate();
		int i = 0;
		std::vector<std::string> names{"occlusionPass", "cascadeShadowPass", "pointLightPass", "geometryPass", "shadersPass", "SSAOPass", "lightningPass", "debugPass", "GUIPass"};
		for (const auto renderPass : mRenderPasses) {
			FUNCTION_BENCHMARK_NAMED_STR("pass: " + names[i]);
			renderPass->render(mRenderData);
			i++;
		}
		Render::TextRenderer::instance()->renderText("FPS: " + std::to_string(Engine::instance()->getFPS()), 10.f, 50.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		Render::TextRenderer::instance()->renderText("dt: " + std::to_string(Engine::instance()->getDeltaTime()), 10.f, 80.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));

		prepareDataForNextFrame();
	}

	void RenderSystem::debugUpdate(float dt) {
		return;
	}

	void RenderSystem::prepareDataForNextFrame() {
		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> dir;
		{
			FUNCTION_BENCHMARK_NAMED(dirties_copy);
			dirtiesMutex.lock();
			dir = dirties;
			for (auto& [id, entities] : dirties) {
				std::erase_if(entities, [](std::pair<ecss::EntityId, uint8_t>& val) {
					return val.second-- <= 1;
				});
			}
			dirtiesMutex.unlock();
		}

		//std::unordered_map<ecss::ECSType, ecss::EntitiesRanges> dir;
		//{
		//	FUNCTION_BENCHMARK_NAMED(dirties_copy);
		//	dirtiesMutex.lock();
		//	dir = dirtiesRanges;
		//	/*for (auto& [id, entities] : dirties) {
		//		std::erase_if(entities, [](std::pair<ecss::EntityId, uint8_t>& val) {
		//			return val.second-- <= 1;
		//		});
		//	}*/
		//	dirtiesMutex.unlock();
		//}

		//prepare data for next frame
		{
			FUNCTION_BENCHMARK_NAMED(copy_components_transform_armat);
			

			for (auto& [id, entities] : dir) {
				ecss::Ranges<ecss::EntityId> range;
				for (auto [entId, _] : entities) {
					range.insert(entId);
				}

				if (id == getDirtyId<ComponentsModule::TransformMatComp>()) {//todo support deleting
					for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::TransformMatComp>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<ComponentsModule::TransformMatComp>(entId, *comp);
					}
				}
				else if (id == getDirtyId<ComponentsModule::ArmatureBonesComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<ComponentsModule::ArmatureBonesComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<ComponentsModule::ArmatureBonesComponent>(entId, *comp);
					}
				}
				else if (id == getDirtyId<MeshComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<MeshComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<MeshComponent>(entId, *comp);
					}
				}
				else if (id == getDirtyId<MaterialComponent>()) {
					for (auto [entId, comp] : ECSHandler::registry().view<MaterialComponent>(range)) {
						ECSHandler::drawRegistry(mRenderData.currentRegistry).addComponent<MaterialComponent>(entId, *comp);
					}
				}
			}
			{
				FUNCTION_BENCHMARK_NAMED(copy_components);
				// todo i need some system which will handle dirty for render components, may be it should be render system itself, then when i updating draw registry
				ECSHandler::drawRegistry(mRenderData.currentRegistry).insert<OutlineComponent>(*ECSHandler::registry().getComponentContainer<OutlineComponent>());
				ECSHandler::drawRegistry(mRenderData.currentRegistry).insert<ComponentsModule::OccludedComponent>(*ECSHandler::registry().getComponentContainer<ComponentsModule::OccludedComponent>());
			}
		}
	}

	void RenderSystem::notify(Task task) {
		if (task.type == TRAHSFORM_RELOADED) {
			ECSHandler::registry().addComponent<ComponentsModule::TransformMatComp>(task.entity, std::move(static_cast<TransformComponent*>(task.customData)->getTransform()));
			markDirty<ComponentsModule::TransformMatComp>(task.entity);
		}
		else if (task.type == ARMATURE_UPDATED) {
			markDirty<ComponentsModule::ArmatureBonesComponent>(task.entity);
		}
		else if (task.type == MATERIAL_UPDATED) {
			markDirty<ComponentsModule::MaterialComponent>(task.entity);
		}
		else if (task.type == MESH_UPDATED) {
			markDirty<ComponentsModule::MeshComponent>(task.entity);
		}
	}

	RenderSystem::~RenderSystem() {
		for (const auto renderPass : mRenderPasses) {
			delete renderPass;
		}
	}
}
