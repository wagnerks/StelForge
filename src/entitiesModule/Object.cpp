#include "Object.h"

#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/TransformComponent.h"

using namespace Engine::EntitiesModule;

Object::Object(size_t entId) : Entity<Object>(entId) {
	auto tc = addComponent<TransformComponent>();
	auto rc = addComponent<IsDrawableComponent>();
}