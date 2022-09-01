#include "LODSystem.h"

#include "componentsModule/LodComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/ProjectionComponent.h"
#include "core/Engine.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/ECSHandler.h"

using namespace GameEngine::SystemsModule;

void LODSystem::update(float_t dt) {
	const auto playerCamera = Engine::getInstance()->getCamera(); //todo entity player should have camera component
	if (!playerCamera) {
		return;
	}

	for (auto& lodObject : *ecsModule::ECSHandler::componentManagerInstance()->getComponentContainer<LodComponent>()) {
		auto transform = ecsModule::ECSHandler::componentManagerInstance()->getComponent<TransformComponent>(lodObject.getOwnerId());
		if (!transform) {
			continue;
		}

		float value = 0.f;
		if (lodObject.getLodType() == eLodType::SCREEN_SPACE) {
			if (const auto modelComponent = ecsModule::ECSHandler::componentManagerInstance()->getComponent<ModelComponent>(lodObject.getOwnerId())) {
				if (const auto model = modelComponent->getModel()) {
					for (auto& mesh : model->getMeshes()){
						value = std::max(value, calculateScreenSpaceArea(mesh.get(), playerCamera, transform));
					}
				}
			}
		}
		else if (lodObject.getLodType() == eLodType::DISTANCE) {
			value = calculateDistanceToMesh(playerCamera, transform);
		}

		int lodLevel = 0;
		for (auto level : lodObject.getLodLevelValues()) {
			if (level < value) {
				break;
			}
			lodLevel++;
		}

		lodObject.setCurrentLodValue(value);
		lodObject.setLodLevel(lodLevel);
	}
}

float LODSystem::calculateScreenSpaceArea(const ModelModule::Mesh* mesh, const Camera* camera, TransformComponent* meshTransform) {
	if (!mesh || !camera || !meshTransform) {
		return 0.f;
	}

	auto t = meshTransform->getTransform();
	t = camera->getComponent<ProjectionComponent>()->getProjection().getProjectionsMatrix() * camera->getComponent<TransformComponent>()->getViewMatrix() * t;
	auto d = glm::distance(camera->getComponent<TransformComponent>()->getPos(true), meshTransform->getPos(true));


	glm::vec3 globalScale = meshTransform->getScale(true);/*{
		sqrt(t[0][0] * t[0][0] + t[1][0] * t[1][0] + t[2][0] * t[2][0]),
		sqrt(t[0][1] * t[0][1] + t[1][1] * t[1][1] + t[2][1] * t[2][1]),
		sqrt(t[0][2] * t[0][2] + t[1][2] * t[1][2] + t[2][2] * t[2][2])
	};*/

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

float LODSystem::calculateDistanceToMesh(const Camera* camera, TransformComponent* meshTransform) {
	return glm::distance(camera->getComponent<TransformComponent>()->getPos(true), meshTransform->getPos(true));
}
