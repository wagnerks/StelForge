#include "ModelEntity.h"


#include "componentsModule/ModelComponent.h"
#include "componentsModule/RenderComponent.h"
#include "componentsModule/TransformComponent.h"
#include "ecsModule/EntityManager.h"

using namespace Engine::EntitiesModule;

Model::Model(size_t entId) : Entity<Model>(entId) {
	setNodeId("model");
	auto tc = addComponent<TransformComponent>();
	auto rc = addComponent<RenderComponent>();
}