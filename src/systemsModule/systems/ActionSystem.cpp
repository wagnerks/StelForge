#include "ActionSystem.h"

#include "componentsModule/ActionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

void Engine::SystemsModule::ActionSystem::update(float dt) {
	for (auto [entity, action, transform] : ECSHandler::registry().getComponentsArray<ComponentsModule::ActionComponent, TransformComponent>()) {
		if (action.progress == 0.f) {
			action.initialPos = transform.getPos();
		}
		action.time = 60.f;
		action.progress += dt / action.time;
		action.loop = true;

		if (action.progress >= 1.f) {
			if (action.loop) {
				action.progress = 0.f;
			}
			else {
				action.progress = 1.f;
			}
		}

		auto pos = action.initialPos;
		pos.y += std::sin(action.progress * Math::pi<float>() * 120.f) * 2.f;
		pos.x += std::sin(action.progress * Math::pi<float>()) * 200.f;
		transform.setPos(pos);
		
	}
}
