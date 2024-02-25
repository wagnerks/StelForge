#include "AABBSystem.h"

#include <random>

#include "systemsModule/SystemManager.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "core/ThreadPool.h"


void SFE::SystemsModule::AABBSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ECSHandler::registry().forEach<ModelComponent, ComponentsModule::AABBComponent, TransformComponent>(entitiesToProcess, [this](auto entity, ModelComponent* modelComp, ComponentsModule::AABBComponent* aabbcomp, TransformComponent* transform) {
		if (!modelComp || !aabbcomp) {
			return;
		} 
		auto& model = modelComp->getModel(0);
		if (aabbcomp->aabbs.empty()) {
			aabbcomp->mtx.lock();
			aabbcomp->aabbs.resize(model.meshes.size());
			aabbcomp->mtx.unlock();
		}

		
		auto& transformMatrix = transform->getTransform();
		for (size_t i = 0; i < aabbcomp->aabbs.size(); i++) {
			auto& aabb = aabbcomp->aabbs[i];
			static constexpr auto I = Math::Vec3{ 1.f, 0.f, 0.f };
			static constexpr auto J = Math::Vec3{ 0.f, 1.f, 0.f };
			static constexpr auto K = Math::Vec3{ 0.f, 0.f, 1.f };

			const auto& meshHandle = model.meshes[i];
			const Math::Vec3 right	 { transformMatrix[0] * meshHandle->getBounds().extents.x};
			const Math::Vec3 up		 { transformMatrix[1] * meshHandle->getBounds().extents.y};
			const Math::Vec3 forward {-transformMatrix[2] * meshHandle->getBounds().extents.z};

			aabbcomp->mtx.lock();
			aabb.center = transformMatrix * Math::Vec4{meshHandle->getBounds().center, 1.f};
			aabb.extents = { std::abs(Math::dot(I, right)) + std::abs(Math::dot(I, up)) + std::abs(Math::dot(I, forward)),
				std::abs(Math::dot(J, right)) + std::abs(Math::dot(J, up)) + std::abs(Math::dot(J, forward)),
				std::abs(Math::dot(K, right)) + std::abs(Math::dot(K, up)) + std::abs(Math::dot(K, forward))
			};
			aabbcomp->mtx.unlock();
		}
		
		updateDependents(entity);
	});
}
