#include "TransformSystem.h"

#include "CameraSystem.h"
#include "RenderSystem.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

using namespace SFE::SystemsModule;

void TransformSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ECSHandler::registry().forEachAsync<TransformComponent>(entitiesToProcess, [this](ecss::SectorId entity, TransformComponent* transform) {
		if (!transform) {
			return;
		}

		transform->reloadTransform();
		TasksManager::instance()->notify({ entity, TRAHSFORM_RELOADED, transform });
		
	});
}