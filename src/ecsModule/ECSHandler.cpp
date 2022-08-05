#include "ECSHandler.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"
#include "systemsModule/TransformSystem.h"

using namespace ecsModule;

SystemManager* ECSHandler::systemManagerInstance() {
	return getInstance()->systemManager;
}

EntityManager* ECSHandler::entityManagerInstance() {
	return getInstance()->entityManager;
}

ComponentManager* ECSHandler::componentManagerInstance() {
	return getInstance()->componentManager;
}

ECSHandler* ECSHandler::getInstance() {
	if (!instance) {
		instance = new ECSHandler();
		instance->init();
	}

	return instance;
}

void ECSHandler::terminate() {
	delete instance;
}

void ECSHandler::init() {
	memoryManager = new GameEngine::MemoryModule::MemoryManager(ECS_GLOBAL_MEMORY_CAPACITY);

	systemManager = new SystemManager(memoryManager);
	componentManager = new ComponentManager(memoryManager);
	entityManager = new EntityManager(memoryManager);

	systemManager->addSystem<GameEngine::SystemsModule::TransformSystem>();
}

ECSHandler::~ECSHandler() {
	delete systemManager;
	delete entityManager;
	delete componentManager;

	delete memoryManager;
}