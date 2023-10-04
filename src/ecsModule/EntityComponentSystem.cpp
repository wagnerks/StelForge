#include "EntityComponentSystem.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"

using namespace ECS;

EntityComponentSystem::EntityComponentSystem() {
	mMemoryManager = new Memory::ECSMemoryStack(ECS_GLOBAL_MEMORY_CAPACITY);


	mSystemManager = new (mMemoryManager->allocate(sizeof(SystemManager) + alignof(SystemManager)))SystemManager(mMemoryManager);
	mComponentManager = new (mMemoryManager->allocate(sizeof(ComponentManager) + alignof(ComponentManager)))ComponentManager(mMemoryManager);
	mEntityManager = new (mMemoryManager->allocate(sizeof(EntityManager) + alignof(EntityManager)))EntityManager(mMemoryManager, this);
}

EntityComponentSystem::~EntityComponentSystem() {
	mSystemManager->~SystemManager();
	mComponentManager->clearComponents();
	mEntityManager->~EntityManager();
	mComponentManager->~ComponentManager();

	delete mMemoryManager; //will clear all the memory which was allocated by ecs
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
