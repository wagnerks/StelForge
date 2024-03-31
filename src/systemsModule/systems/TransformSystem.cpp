#include "TransformSystem.h"

#include "CameraSystem.h"
#include "RenderSystem.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

using namespace SFE::SystemsModule;

void TransformSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
	ecss::EntityId curCamera = ecss::INVALID_ID;
	if (auto cameraSys = ECSHandler::systemManager().getSystem<CameraSystem>()) {
		curCamera = cameraSys->getCurrentCamera();
	}

	ECSHandler::registry().forEachAsync<TransformComponent>(entitiesToProcess, [this, curCamera](ecss::SectorId entity, TransformComponent* transform) {
		if (!transform) {
			return;
		}

		transform->reloadTransform();

		if (entity == curCamera) {
			ECSHandler::registry().getComponent<CameraComponent>(entity)->updateFrustum(transform->getViewMatrix());
			TasksManager::instance()->notify({ entity, CAMERA_UPDATED });
		}
		TasksManager::instance()->notify({ entity, TRAHSFORM_RELOADED, transform });
		
	});
}