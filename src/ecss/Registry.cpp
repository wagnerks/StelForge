#include "Registry.h"

#include <map>

#include "base/ComponentBase.h"

namespace ecss {
	Registry::Registry() {}

	Registry::~Registry() {
		clear();

		std::map<void*, bool> deleted;

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

	EntityHandle Registry::takeEntity() {
		auto id = getNewId();
		mEntities.insert(mEntities.begin() + id, id);

		return { id };
	}

	EntityHandle Registry::getEntity(EntityId entityId) const {
		if (std::find(mEntities.begin(), mEntities.end(), entityId) != mEntities.end()) {
			return { entityId };
		}

		return { INVALID_ID };
	}

	void Registry::destroyEntity(const EntityHandle& entity) {
		if (!entity) {
			return;
		}

		const auto id = entity.getID();
		if (std::find(mEntities.begin(), mEntities.end(), id) == mEntities.end()) {
			return;
		}

		if (id != *mEntities.rbegin()) {//if entity was removed not from end - add its id to the list
			mFreeEntities.insert(id);
		}

		mEntities.erase(std::find(mEntities.begin(), mEntities.end(),id));
		destroyComponents(id);
	}

	const std::vector<EntityId>& Registry::getAllEntities() {
		return mEntities;
	}

	EntityId Registry::getNewId() {
		if (!mFreeEntities.empty()) {
			const auto id = *mFreeEntities.begin();
			mFreeEntities.erase(id);

			return id;
		}

		return static_cast<EntityId>(mEntities.size());
	}
}