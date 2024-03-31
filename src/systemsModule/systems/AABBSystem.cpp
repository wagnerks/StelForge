#include "AABBSystem.h"

#include <random>

#include "systemsModule/SystemManager.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "multithreading/ThreadPool.h"


void SFE::SystemsModule::AABBSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ECSHandler::registry().forEachAsync<ComponentsModule::AABBComponent, const TransformComponent>(entitiesToProcess, [this](auto entity, ComponentsModule::AABBComponent* aabbcomp, const TransformComponent* transform) {
		if (!aabbcomp) {
			return;
		}
		if (aabbcomp->defaultAabbs.empty()) {//todo move it to some initial aabb component place
			auto modelComp = ECSHandler::registry().getComponent<ModelComponent>(entity);
			if (!modelComp) {
				return;
			}
			auto& model = modelComp->getModel(0);
			aabbcomp->mtx.lock();
			aabbcomp->defaultAabbs.reserve(model.meshes.size());
			for (auto& mesh : model.meshes) {
				aabbcomp->defaultAabbs.emplace_back(mesh->aabb);
			}
			aabbcomp->mtx.unlock();
		}

		if (aabbcomp->aabbs.empty()) {
			aabbcomp->mtx.lock();
			aabbcomp->aabbs.resize(aabbcomp->defaultAabbs.size());
			aabbcomp->mtx.unlock();
		}

		static constexpr auto I = Math::Vec3{ 1.f, 0.f, 0.f };
		static constexpr auto J = Math::Vec3{ 0.f, 1.f, 0.f };
		static constexpr auto K = Math::Vec3{ 0.f, 0.f, 1.f };

		auto& transformMatrix = transform->getTransform();
		aabbcomp->mtx.lock();
		for (size_t i = 0; i < aabbcomp->aabbs.size(); i++) {
			auto& aabb = aabbcomp->aabbs[i];

			const auto& defaultAABB = aabbcomp->defaultAabbs[i];
			
			const Math::Vec3 right	 { transformMatrix[0] * defaultAABB.extents.x};
			const Math::Vec3 up		 { transformMatrix[1] * defaultAABB.extents.y};
			const Math::Vec3 forward {-transformMatrix[2] * defaultAABB.extents.z};

			aabb.center = transformMatrix * Math::Vec4{defaultAABB.center, 1.f};
			aabb.extents = { std::abs(Math::dot(I, right)) + std::abs(Math::dot(I, up)) + std::abs(Math::dot(I, forward)),
				std::abs(Math::dot(J, right)) + std::abs(Math::dot(J, up)) + std::abs(Math::dot(J, forward)),
				std::abs(Math::dot(K, right)) + std::abs(Math::dot(K, up)) + std::abs(Math::dot(K, forward))
			};
		}
		aabbcomp->mtx.unlock();
		TasksManager::instance()->notify({ entity, AABB_UPDATED });
	});
}
