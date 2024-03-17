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


void SFE::SystemsModule::AABBSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
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
				auto minAABB = SFE::Math::Vec3(std::numeric_limits<float>::max());
				auto maxAABB = SFE::Math::Vec3(std::numeric_limits<float>::min());

				for (auto vertex : mesh->mesh.vertices) {
					vertex.position = mesh->transform * SFE::Math::Vec4(vertex.position, 1.f);
					minAABB.x = std::min(minAABB.x, vertex.position.x);
					minAABB.y = std::min(minAABB.y, vertex.position.y);
					minAABB.z = std::min(minAABB.z, vertex.position.z);

					maxAABB.x = std::max(maxAABB.x, vertex.position.x);
					maxAABB.y = std::max(maxAABB.y, vertex.position.y);
					maxAABB.z = std::max(maxAABB.z, vertex.position.z);
				}

				aabbcomp->defaultAabbs.emplace_back(minAABB / 100.f, maxAABB / 100.f);
				//aabbcomp->defaultAabbs.emplace_back(minAABB, maxAABB);
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

		updateDependents(entity);
	});
}
