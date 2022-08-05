#include "TransformSystem.h"

#include "componentsModule/TransformComponent.h"
#include "ecsModule/ComponentsManager.h"
#include "ecsModule/EntityBase.h"
#include "ecsModule/EntityManager.h"

using namespace GameEngine::SystemsModule;

void TransformSystem::update(float_t dt) {
	auto transforms = ecsModule::ECSHandler::componentManagerInstance()->getComponentContainer<TransformComponent>();

	for (auto& transform : *transforms) {
		transform.reloadTransform();
	}
}