#include "TransformSystem.h"

#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "..\ecss\Registry.h"

using namespace Engine::SystemsModule;

void TransformSystem::update(float_t dt) {
	for (auto [transform] : ECSHandler::registry()->getComponentsArray<TransformComponent>()) {
		if(!&transform) {
			continue;
		}
		transform.reloadTransform();
	}
}