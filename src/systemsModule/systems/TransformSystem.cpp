#include "TransformSystem.h"

#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

using namespace SFE::SystemsModule;

void TransformSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ECSHandler::registry().forEach<TransformComponent>(entitiesToProcess, [this](ecss::SectorId entity, TransformComponent* transform) {
		if (!transform) {
			return;
		}

		transform->reloadTransform();
		updateDependents(entity);
	});
}

void TransformSystem::addDirtyComp(ecss::SectorId entity) {
	onDependentParentUpdate(mType, entity);//use its own dependence container
}