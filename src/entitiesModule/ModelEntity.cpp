#include "ModelEntity.h"

#include "componentsModule/LodComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/RenderComponent.h"
#include "componentsModule/TransformComponent.h"

using namespace GameEngine::EntitiesModule;

Model::Model(size_t entId) : Entity<Model>(entId) {
	setStringId("model");
	auto tc = addComponent<TransformComponent>();
	auto mc = addComponent<ModelComponent>();
	auto lc = addComponent<LodComponent>();
	auto rc = addComponent<RenderComponent>();
}