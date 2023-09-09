#include "ECSHandler.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"

#include "systemsModule/CameraSystem.h"
#include "systemsModule/LODSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"
#include "systemsModule/TransformSystem.h"

using namespace ecsModule;

SystemManager* ECSHandler::systemManagerInstance() {
	return instance()->systemManager;
}

EntityManager* ECSHandler::entityManagerInstance() {
	return instance()->entityManager;
}

ComponentManager* ECSHandler::componentManagerInstance() {
	return instance()->componentManager;
}

void ECSHandler::init() {
	memoryManager = new Engine::MemoryModule::MemoryManager(ECS_GLOBAL_MEMORY_CAPACITY);

	systemManager = new SystemManager(memoryManager);
	componentManager = new ComponentManager(memoryManager);
	entityManager = new EntityManager(memoryManager);
}

void ECSHandler::initSystems() {
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

	systemManager->addSystem<Engine::SystemsModule::RenderSystem>(Engine::RenderModule::Renderer::instance());
	systemManager->setSystemPriority<Engine::SystemsModule::RenderSystem>(eSystemsPriority::RENDER_SYSTEM);
}

ECSHandler::~ECSHandler() {
	delete systemManager;
	delete entityManager;
	delete componentManager;

	delete memoryManager;
}

Engine::MemoryModule::MemoryManager* ECSHandler::getMemoryManager() const {
	return memoryManager;
}
