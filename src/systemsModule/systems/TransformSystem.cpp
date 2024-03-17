#include "TransformSystem.h"

#include "RenderSystem.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

using namespace SFE::SystemsModule;

void TransformSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ECSHandler::registry().forEachAsync<TransformComponent>(entitiesToProcess, [this](ecss::SectorId entity, TransformComponent* transform) {
		if (!transform) {
			return;
		}

		transform->reloadTransform();
		ECSHandler::registry().addComponent<ComponentsModule::TransformMatComp>(entity)->mTransform = transform->getTransform();
		if (auto renderSys = ECSHandler::systemManager().getSystem<RenderSystem>()) {
			renderSys->markDirty<ComponentsModule::TransformMatComp>(entity);
		}
		
		updateDependents(entity);
	});
}

void TransformSystem::addDirtyComp(ecss::SectorId entity) {
	onDependentParentUpdate(mType, entity);//use its own dependence container
}