#include "TransformSystem.h"

#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"

using namespace Engine::SystemsModule;

void TransformSystem::update(float_t dt) {
	for (auto [entity, isDirty, transform] : ECSHandler::registry()->getComponentsArray<DirtyTransform, TransformComponent>()) {
		if(!&transform) {
			continue;
		}
		transform.reloadTransform();
	}

	ECSHandler::registry()->getComponentContainer<DirtyTransform>()->clear();
	ECSHandler::registry()->getComponentContainer<DirtyTransform>()->shrinkToFit();
}