#include "EntityComponentSystem.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"

using namespace ecsModule;

EntityComponentSystem::EntityComponentSystem(Engine::MemoryModule::MemoryManager* memoryManager) : mMemoryManager(memoryManager) {
	mSystemManager = new SystemManager(mMemoryManager);
	mComponentManager = new ComponentManager(mMemoryManager);
	mEntityManager = new EntityManager(mMemoryManager);
}

EntityComponentSystem::~EntityComponentSystem() {
	delete mSystemManager;
	delete mEntityManager;
	delete mComponentManager;
}

SystemManager* EntityComponentSystem::getSystemManager() const {
	return mSystemManager;
}

EntityManager* EntityComponentSystem::getEntityManager() const {
	return mEntityManager;
}

ComponentManager* EntityComponentSystem::getComponentManager() const {
	return mComponentManager;
}
