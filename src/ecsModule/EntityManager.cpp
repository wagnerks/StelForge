#include "EntityManager.h"

#include "ComponentsManager.h"
#include "EntityComponentSystem.h"
#include "base/EntityBase.h"

using namespace ECS;


EntityManager::EntityManager(EntityComponentSystem* ecs) : mEcsController(ecs) {}

EntityHandle EntityManager::takeEntity(EntityId id) {
	if (id == INVALID_ID) {
		id = getNewId();
	}

	mEntities.insert(id);

	return {id};
}

EntityHandle EntityManager::getEntity(EntityId entityId) const {
	if (mEntities.contains(entityId)) {
		return { entityId };
	}

	return { INVALID_ID };
}

void EntityManager::destroyEntity(EntityId entityId) {
	if (!mEntities.contains(entityId)) {
		return;
	}

	mEntitiesToDelete.insert(entityId);
}

void EntityManager::destroyEntities() {
	for (const auto entityId : mEntitiesToDelete) {
		mEntities.erase(entityId);
		if (entityId != mEntities.size()) {//if entity was removed not from end - add its id to the list
			mFreeEntities.push_front(entityId);
		}

		mEcsController->getComponentManager()->destroyComponents(entityId);
	}

	mEntitiesToDelete.clear();
}

const std::set<EntityId>& EntityManager::getAllEntities() {
	return mEntities;
}

EntityId EntityManager::getNewId() {
	if (!mFreeEntities.empty()) {
		const auto id = mFreeEntities.front();
		mFreeEntities.pop_front();

		return id;
	}

	return static_cast<EntityId>(mEntities.size());
}