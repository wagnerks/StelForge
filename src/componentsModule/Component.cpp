#include "Component.h"

using namespace GameEngine::ComponentsModule;

Component::Component(ComponentHolder* holder) {
	owner = holder;
}