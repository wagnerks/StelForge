#include "ECSHandler.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"
#include "core/Engine.h"
#include "systemsModule/LODSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"
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
}

void ECSHandler::initSystems() {
	if(!systemManager) {
		return;
	}

	systemManager->addSystem<GameEngine::SystemsModule::TransformSystem>();
	systemManager->setSystemPriority<GameEngine::SystemsModule::TransformSystem>(eSystemsPriority::TRANSFORM_SYSTEM);
	systemManager->setSystemUpdateInterval<GameEngine::SystemsModule::TransformSystem>(1/60.f);

	systemManager->addSystem<GameEngine::SystemsModule::LODSystem>();
	systemManager->setSystemPriority<GameEngine::SystemsModule::LODSystem>(eSystemsPriority::LOD_SYSTEM);
	systemManager->setSystemUpdateInterval<GameEngine::SystemsModule::LODSystem>(1/60.f);

	systemManager->addSystem<GameEngine::SystemsModule::RenderSystem>(GameEngine::Engine::getInstance()->getRenderer());
	systemManager->setSystemPriority<GameEngine::SystemsModule::RenderSystem>(eSystemsPriority::RENDER_SYSTEM);
}

ECSHandler::~ECSHandler() {
	delete systemManager;
	delete entityManager;
	delete componentManager;

	delete memoryManager;
}
