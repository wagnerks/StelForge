#include "LODSystem.h"

#include <ext/scalar_constants.hpp>

#include "CameraSystem.h"
#include "systemsModule/SystemManager.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "ecss/Registry.h"
#include "componentsModule/IsDrawableComponent.h"
#include "logsModule/logger.h"

using namespace SFE::SystemsModule;

void LODSystem::update(float_t dt) {
	const auto playerCamera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	if (!playerCamera) {
		return;
	}

	auto playerPos = ECSHandler::registry().getComponent<TransformComponent>(playerCamera)->getPos(true);
	for (const auto& [entity, isDraw, transform, lodObject] : ECSHandler::registry().getComponentsArray<const IsDrawableComponent, const TransformComponent, ModelComponent>()) {
		if (!isDraw) {
			continue;
		}
		if (!lodObject) {
			continue;
		}
		float value = 0.f;
		//if (lodObject.getLodType() == ComponentsModule::eLodType::SCREEN_SPACE) {
		//	//if (const auto modelComponent = ecss::ECSHandler::registry().getComponent<ModelComponent>(lodObject.getEntityId())) {
		//	//	if (const auto model = modelComponent->getModel()) {
		//	//		/*for (auto& mesh : model->getMeshes()){
		//	//			value = std::max(value, calculateScreenSpaceArea(&mesh, playerCamera, transform));
		//	//		}*/
		//	//	}
		//	//}
		//}
		//else if (lodObject.getLodType() == ComponentsModule::eLodType::DISTANCE) {
		value = Math::distanceSqr(playerPos, transform->getPos(true));
		//}

		int lodLevel = 0;
		for (auto level : lodObject->mLOD.getLodLevelValues()) {
			if (level < value) {
				break;
			}
			lodLevel++;
		}

		lodObject->mLOD.setLodLevel(lodLevel);
		lodObject->mLOD.setCurrentLodValue(value);
	}
}

//float LODSystem::calculateScreenSpaceArea(const AssetsModule::Mesh* mesh, const ecss::EntityHandle& camera, TransformComponent* meshTransform) {
//	if (!mesh || !camera || !meshTransform) {
//		return 0.f;
//	}
//
//	/*auto t = meshTransform->getTransform();
//	t = camera->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix() * camera->getComponent<TransformComponent>()->getViewMatrix() * t;*/
//	auto p1 = ECSHandler::registry().getComponent<ComponentsModule::TransformComponent>(camera)->getPos(true);
//	auto p2 = meshTransform->getPos(true);
//
//	auto d = Math::distance(p1, p2);
//
//	Math::Vec3 globalScale = meshTransform->getGlobalScale();
//
//	if (d == 0.f) {
//		globalScale.x = std::numeric_limits<float>::max();
//		globalScale.y = std::numeric_limits<float>::max();
//		globalScale.z = std::numeric_limits<float>::max();
//	}
//	else if (d < 0.f) {
//		globalScale.x = 0.f;
//		globalScale.y = 0.f;
//		globalScale.z = 0.f;
//	}
//	else {
//		globalScale /= d;
//	}
//
//	const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);
//	auto spaceRadius = /*mesh->bounds->radius * */(maxScale * 0.5f);
//
//	return spaceRadius * spaceRadius * Math::pi<float>();
//}

float LODSystem::calculateDistanceToMesh(const Math::Vec3& cameraPos, const Math::Vec3& meshPos) {
	return Math::distance(cameraPos, meshPos);
}
