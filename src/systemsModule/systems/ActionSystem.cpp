#include "ActionSystem.h"

#include "componentsModule/ActionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"

void SFE::SystemsModule::ActionSystem::update(float dt) {
	static std::map<unsigned, int> step;
	
	for (auto [entity, action, transform] : ECSHandler::registry().getComponentsArray<ComponentsModule::ActionComponent, TransformComponent>()) {
		if (action->progress == 0.f) {
			action->initialPos = transform->getPos();
		}
		action->time = 60.f;
		action->progress += dt / action->time;
		action->loop = true;

		if (action->progress >= 1.f) {
			step[entity]++;
			if (step[entity] == 4) {
				step[entity] = 0;
			}
			if (action->loop) {
				action->progress = 0.f;
			}
			else {
				action->progress = 1.f;
			}
			continue;
		}

		switch(step[entity]) {
		case 0: {
			auto pos = action->initialPos;
			pos.y += std::sin(action->progress * Math::pi<float>() * 60.f) * 0.5f;
			pos.x += action->progress * 190.f;
			transform->setPos(pos);
			break;
		}
		case 1: {
			auto pos = action->initialPos;
			pos.y += std::sin(action->progress * Math::pi<float>() * 60.f) * 0.5f;
			pos.z += action->progress * 70.f;
			transform->setPos(pos);
			break;
		}
		case 2: {
			auto pos = action->initialPos;
			pos.y += std::sin(action->progress * Math::pi<float>() * 60.f) * 0.5f;
			pos.x -= action->progress * 190.f;
			transform->setPos(pos);
			break;
		}
		case 3: {
			auto pos = action->initialPos;
			pos.y += std::sin(action->progress * Math::pi<float>() * 60.f) * 0.5f;
			pos.z -= action->progress * 70.f;
			transform->setPos(pos);
			break;
		}
		default:;
		}
		
		
	}
}
