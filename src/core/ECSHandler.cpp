#include "ECSHandler.h"

#include "componentsModule/FrustumComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/ShaderComponent.h"
#include "componentsModule/TreeComponent.h"

#include "systemsModule/CameraSystem.h"
#include "systemsModule/LODSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/ShaderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"
#include "systemsModule/TransformSystem.h"


ECSHandler::ECSHandler() {
	mSystemManager = new ecss::SystemManager();
	mRegistry = new ecss::Registry();
}

ecss::SystemManager* ECSHandler::systemManager() {
	return instance()->mSystemManager;
}

ecss::Registry* ECSHandler::registry() {
	return instance()->mRegistry;
}

void ECSHandler::initSystems() {
	//mRegistry->initCustomComponentsContainer<TreeComponent, TransformComponent, ModelComponent, IsDrawableComponent>();
	mRegistry->initCustomComponentsContainer<IsDrawableComponent, TransformComponent, ModelComponent>();


	mSystemManager->addSystem<Engine::SystemsModule::CameraSystem>();

	mSystemManager->addSystem<Engine::SystemsModule::TransformSystem>();
	mSystemManager->setSystemPriority<Engine::SystemsModule::TransformSystem>(eSystemsPriority::TRANSFORM_SYSTEM);
	mSystemManager->setSystemUpdateInterval<Engine::SystemsModule::TransformSystem>(1 / 60.f);

	mSystemManager->addSystem<Engine::SystemsModule::LODSystem>();
	mSystemManager->setSystemPriority<Engine::SystemsModule::LODSystem>(eSystemsPriority::LOD_SYSTEM);
	mSystemManager->setSystemUpdateInterval<Engine::SystemsModule::LODSystem>(1.f);

	mSystemManager->addSystem<Engine::SystemsModule::ShaderSystem>();
	mSystemManager->setSystemPriority<Engine::SystemsModule::ShaderSystem>(eSystemsPriority::SHADERS_SYSTEM);
	mSystemManager->setSystemUpdateInterval<Engine::SystemsModule::ShaderSystem>(1 / 60.f);

	mSystemManager->addSystem<Engine::SystemsModule::RenderSystem>(Engine::RenderModule::Renderer::instance());
	mSystemManager->setSystemPriority<Engine::SystemsModule::RenderSystem>(eSystemsPriority::RENDER_SYSTEM);
}