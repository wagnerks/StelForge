#include "Registry.h"

#include "base/ComponentBase.h"

namespace ecss {
	Registry::Registry() {
		mComponentsArraysMap.resize(StaticTypeCounter<ComponentInterface>::getCount(), nullptr);
	}

	Registry::~Registry() {
		clear();

		std::unordered_map<void*, bool> deleted;

		for (const auto container : mComponentsArraysMap) {
			if (!container || deleted[container]) {//skip not created and containers of multiple components
				continue;
			}

			delete container;
			deleted[container] = true;
		}
	}

	void Registry::clear() {
		for (const auto compContainer : mComponentsArraysMap) {
			if (!compContainer) {
				continue;
			}
			compContainer->clear();
		}

		mEntities.clear();
		mFreeEntities.clear();
	}

	void Registry::destroyComponents(const EntityHandle& entity) const {
		for (const auto compContainer : mComponentsArraysMap) {
			if (!compContainer) {
				continue;
			}
			compContainer->destroySector(entity.getID());
		}
	}

	EntityHandle Registry::takeEntity(EntityId id) {
		if (id == INVALID_ID) {
			id = getNewId();
		}

		mEntities.insert(id);

		return { id };
	}

	EntityHandle Registry::getEntity(EntityId entityId) const {
		if (mEntities.contains(entityId)) {
			return { entityId };
		}

		return { INVALID_ID };
	}

	void Registry::destroyEntity(const EntityHandle& entity) {
		if (!entity) {
			return;
		}

		if (!mEntities.contains(entity.getID())) {
			return;
		}

		if (entity.getID() != *mEntities.rbegin()) {//if entity was removed not from end - add its id to the list
			mFreeEntities.push_front(entity.getID());
		}

		mEntities.erase(entity.getID());
		destroyComponents(entity.getID());
	}

	const std::set<EntityId>& Registry::getAllEntities() {
		return mEntities;
	}

	EntityId Registry::getNewId() {
		if (!mFreeEntities.empty()) {
			const auto id = mFreeEntities.front();
			mFreeEntities.pop_front();

			return id;
		}

		return static_cast<EntityId>(mEntities.size());
	}
}