#include "ECSHandler.h"

#include "ecsModule/SystemManager.h"

#include "systemsModule/CameraSystem.h"
#include "systemsModule/LODSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/ShaderSystem.h"
#include "systemsModule/SystemsPriority.h"
#include "systemsModule/TransformSystem.h"


ECSHandler::ECSHandler() {
	ECSMemoryManager = new Engine::MemoryModule::MemoryManager(ecsModule::ECS_GLOBAL_MEMORY_CAPACITY);
	ECS = new ecsModule::EntityComponentSystem(ECSMemoryManager);
}

ecsModule::SystemManager* ECSHandler::systemManagerInstance() {
	return instance()->ECS->getSystemManager();
}

ecsModule::EntityManager* ECSHandler::entityManagerInstance() {
	return instance()->ECS->getEntityManager();
}

ecsModule::ComponentManager* ECSHandler::componentManagerInstance() {
	return instance()->ECS->getComponentManager();
}

void ECSHandler::initSystems() {
	if (!ECS) {
		return;
	}
	auto systemManager = ECS->getSystemManager();
	if (!systemManager) {
		return;
	}


	systemManager->addSystem<Engine::SystemsModule::CameraSystem>();

	systemManager->addSystem<Engine::SystemsModule::TransformSystem>();
	systemManager->setSystemPriority<Engine::SystemsModule::TransformSystem>(eSystemsPriority::TRANSFORM_SYSTEM);
	systemManager->setSystemUpdateInterval<Engine::SystemsModule::TransformSystem>(1 / 60.f);

	systemManager->addSystem<Engine::SystemsModule::LODSystem>();
	systemManager->setSystemPriority<Engine::SystemsModule::LODSystem>(eSystemsPriority::LOD_SYSTEM);
	systemManager->setSystemUpdateInterval<Engine::SystemsModule::LODSystem>(1 / 60.f);

	systemManager->addSystem<Engine::SystemsModule::ShaderSystem>();
	systemManager->setSystemPriority<Engine::SystemsModule::ShaderSystem>(eSystemsPriority::SHADERS_SYSTEM);
	systemManager->setSystemUpdateInterval<Engine::SystemsModule::ShaderSystem>(1 / 60.f);

	systemManager->addSystem<Engine::SystemsModule::RenderSystem>(Engine::RenderModule::Renderer::instance());
	systemManager->setSystemPriority<Engine::SystemsModule::RenderSystem>(eSystemsPriority::RENDER_SYSTEM);
}

ECSHandler::~ECSHandler() {
	delete ECS;
	delete ECSMemoryManager;
}
