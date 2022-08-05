#include "EntityManager.h"

#include "EntityBase.h"

using namespace ecsModule;


EntityManager::EntityManager(GameEngine::MemoryModule::MemoryManager* memoryManager) : GlobalMemoryUser(memoryManager) {
}

EntityManager::~EntityManager() {
	for (auto& ec : mEntityContainers) {
		delete ec.second;
	}
}

size_t EntityManager::acquireEntityId(EntityInterface* entity) {
	size_t i = 0;
	for (; i < mEntities.size(); i++) {
		if (!mEntities[i]) {
			mEntities[i] = entity;
			return i;
		}
	}

	mEntities.resize(mEntities.size() + 1024, nullptr);
	mEntities[i] = entity;

	return i;
}

void EntityManager::releaseEntityId(size_t id) {
	mEntities[id] = nullptr;
}

void EntityManager::destroyEntities() {
	for (const auto entityId : mEntitiesToDelete) {
		EntityInterface* entity = mEntities[entityId];
		
		if (auto it = mEntityContainers.find(entity->getStaticTypeID()); it != mEntityContainers.end()) {
			it->second->destroyElement(entity);
		}
		releaseEntityId(entityId);
	}

	mEntitiesToDelete.clear();
}