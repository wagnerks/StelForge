#include "AABBSystem.h"

#include <mutex>
#include <shared_mutex>

#include "systemsModule/systems/RenderSystem.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "multithreading/ThreadPool.h"


void SFE::SystemsModule::AABBSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
	auto* renderSys = ECSHandler::getSystem<RenderSystem>();
	auto worldLock = renderSys ? renderSys->tryLockWorldRead() : RenderSystem::WorldReadGuard{};
	if (renderSys && !worldLock) {
		return;
	}
	ECSHandler::registry().forEachAsync<ComponentsModule::AABBComponent, const TransformComponent>(entitiesToProcess, [this](auto entity, ComponentsModule::AABBComponent* aabbcomp, const TransformComponent* transform) {
		if (!aabbcomp || !transform) {
			return;
		}
		std::unique_lock aabbLock(aabbcomp->mtx);
		if (aabbcomp->defaultAabbs.empty()) {
			auto modelComp = ECSHandler::registry().pinComponent<const ModelComponent>(entity);
			if (!modelComp) {
				return;
			}
			auto& model = modelComp->getModel(0);
			aabbcomp->defaultAabbs.reserve(model.meshes.size());
			for (auto& mesh : model.meshes) {
				if (!mesh) {
					continue;
				}
				aabbcomp->defaultAabbs.emplace_back(mesh->aabb.transformedBy(mesh->transform));
			}
		}

		if (aabbcomp->aabbs.size() != aabbcomp->defaultAabbs.size()) {
			aabbcomp->aabbs.resize(aabbcomp->defaultAabbs.size());
		}

		const auto transformMatrix = transform->copyTransform();
		const float scale = aabbcomp->aabbScale;
		for (size_t i = 0; i < aabbcomp->aabbs.size(); i++) {
			auto local = aabbcomp->defaultAabbs[i];
			local.extents *= scale;
			aabbcomp->aabbs[i] = local.transformedBy(transformMatrix);
		}
		aabbLock.unlock();
		TasksManager::instance()->notify({ entity, AABB_UPDATED });
	});
}
