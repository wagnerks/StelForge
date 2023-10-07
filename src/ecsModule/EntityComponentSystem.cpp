#include "EntityComponentSystem.h"

#include "ComponentsManager.h"
#include "EntityManager.h"
#include "SystemManager.h"

using namespace ECS;

EntityComponentSystem::EntityComponentSystem() {
	mSystemManager = new SystemManager();
	mComponentManager = new ComponentManager();
	mEntityManager = new EntityManager(this);
}

EntityComponentSystem::~EntityComponentSystem() {
	mComponentManager->clearComponents();

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
