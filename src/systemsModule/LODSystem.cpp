#include "LODSystem.h"

#include <ext/scalar_constants.hpp>

#include "CameraSystem.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/ECSHandler.h"
#include "ecsModule/SystemManager.h"

using namespace Engine::SystemsModule;

void LODSystem::update(float_t dt) {
	const auto playerCamera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	if (!playerCamera) {
		return;
	}

	for (auto& lodObject : *ecsModule::ECSHandler::componentManagerInstance()->getComponentContainer<ModelComponent>()) {
		auto transform = ecsModule::ECSHandler::componentManagerInstance()->getComponent<ComponentsModule::TransformComponent>(lodObject.getOwnerId());
		if (!transform) {
			continue;
		}

		float value = 0.f;
		//if (lodObject.getLodType() == ComponentsModule::eLodType::SCREEN_SPACE) {
		//	//if (const auto modelComponent = ecsModule::ECSHandler::componentManagerInstance()->getComponent<ModelComponent>(lodObject.getOwnerId())) {
		//	//	if (const auto model = modelComponent->getModel()) {
		//	//		/*for (auto& mesh : model->getMeshes()){
		//	//			value = std::max(value, calculateScreenSpaceArea(&mesh, playerCamera, transform));
		//	//		}*/
		//	//	}
		//	//}
		//}
		//else if (lodObject.getLodType() == ComponentsModule::eLodType::DISTANCE) {
		value = calculateDistanceToMesh(playerCamera, transform);
		//}

		int lodLevel = 0;
		for (auto level : lodObject.mLOD.getLodLevelValues()) {
			if (level < value) {
				break;
			}
			lodLevel++;
		}

		lodObject.mLOD.setLodLevel(lodLevel);
		lodObject.mLOD.setCurrentLodValue(value);
	}
}

float LODSystem::calculateScreenSpaceArea(const AssetsModule::Mesh* mesh, const Camera* camera, TransformComponent* meshTransform) {
	if (!mesh || !camera || !meshTransform) {
		return 0.f;
	}

	/*auto t = meshTransform->getTransform();
	t = camera->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix() * camera->getComponent<TransformComponent>()->getViewMatrix() * t;*/
	auto d = glm::distance(camera->getComponent<TransformComponent>()->getPos(true), meshTransform->getPos(true));


	glm::vec3 globalScale = meshTransform->getScale(true);

	if (d == 0.f) {
		globalScale.x = std::numeric_limits<float>::max();
		globalScale.y = std::numeric_limits<float>::max();
		globalScale.z = std::numeric_limits<float>::max();
	}
	else if (d < 0.f) {
		globalScale.x = 0.f;
		globalScale.y = 0.f;
		globalScale.z = 0.f;
	}
	else {
		globalScale /= d;
	}

	const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);
	auto spaceRadius = /*mesh->bounds->radius * */(maxScale * 0.5f);

	return spaceRadius * spaceRadius * glm::pi<float>();
}

float LODSystem::calculateDistanceToMesh(const Camera* camera, ComponentsModule::TransformComponent* meshTransform) {
	return glm::distance(camera->getComponent<ComponentsModule::TransformComponent>()->getPos(true), meshTransform->getPos(true));
}
