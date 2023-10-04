#include "EntityManager.h"

#include <cassert>

#include "ComponentsManager.h"
#include "EntityComponentSystem.h"
#include "base/EntityBase.h"
#include "memory/settings.h"

using namespace ECS;


EntityManager::EntityManager(Memory::ECSMemoryStack* memoryManager, EntityComponentSystem* ecs) : ECSMemoryUser(memoryManager), mEcsController(ecs) {
	for (EntityId i = 0; i < MAX_ENTITIES; i++) {
		mFreeEntities.push_back(i);
	}

	mEntitySize = sizeof(EntityHandle) + alignof(EntityHandle);
	mEntitiesBegin = allocate(MAX_ENTITIES * mEntitySize);
}

EntityManager::~EntityManager() {
	mIsTerminating = true;
	destroyEntities();

	for (auto entity : entities) {
		destroyEntity(entity->getEntityID());
	}

	entities.clear();
}

EntityHandle* EntityManager::createEntity(EntityId id) {
	if (mFreeEntities.empty()) {
		return nullptr;
	}

	if (id == INVALID_ID) {
		id = getNewId();
	}
	else {
		assert(std::find(mFreeEntities.begin(), mFreeEntities.end(), id) == mFreeEntities.end());
	}

	const auto newEntity = new (static_cast<char*>(mEntitiesBegin) + id * mEntitySize)EntityHandle(id);
	insertNewEntity(newEntity, id);

	return newEntity;
}

EntityHandle* EntityManager::getEntity(EntityId entityId) const {
	if (entityId < mEntitiesMap.size() && mEntitiesMap[entityId]) {
		return static_cast<EntityHandle*>(static_cast<void*>(static_cast<char*>(mEntitiesBegin) + entityId * mEntitySize));
	}

	return nullptr;
}

void EntityManager::destroyEntity(EntityId entityId) {
	if (mEntitiesDeletingMap[entityId] || !mEntitiesMap[entityId]) {
		return;
	}
	
	if (mIsTerminating) {
		auto entity = getEntity(entityId);
		mEntitiesMap[entityId] = false;
		entity->~EntityHandle();
	}
	else {
		mEntitiesDeletingMap[entityId] = true;
		mEntitiesToDelete.emplace_back(entityId);
	}
}

void EntityManager::destroyEntities() {
	for (const auto entityId : mEntitiesToDelete) {
		if (auto entity = getEntity(entityId)) {
			if (!mIsTerminating) {
				entities.remove(entity);
				mFreeEntities.push_front(entityId);
			}
			
			mEntitiesMap[entityId] = false;
			entity->~EntityHandle();
			mEcsController->getComponentManager()->destroyComponents(entityId);
			mEntitiesDeletingMap[entityId] = false;
		}
	}

	mEntitiesToDelete.clear();
}

void EntityManager::insertNewEntity(EntityHandle* entity, EntityId id) {
	mEntitiesMap[id] = true;

	if (id >= entities.size()) {
		entities.push_back(entity);
	}
	else {
		auto beg = entities.begin();
		std::advance(beg, id);
		entities.insert(beg, entity);
	}
}

EntityId EntityManager::getNewId() {
	const auto id = mFreeEntities.front();
	mFreeEntities.pop_front();

	return id;
}
