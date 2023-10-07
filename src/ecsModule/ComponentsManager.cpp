#include "ComponentsManager.h"

#include "base/ComponentBase.h"

using namespace ECS;

ComponentManager::ComponentManager() {
	mComponentsArraysMap.resize(StaticTypeCounter<ComponentInterface>::getCount(), nullptr);
}

ComponentManager::~ComponentManager() {
	std::unordered_map<void*, bool> deleted;

	for (const auto container : mComponentsArraysMap) {
		if (!container || deleted[container]) {//skip not created and containers of multiple components
			continue;
		}

		delete container;
		deleted[container] = true;
	}
}

void ComponentManager::clearComponents() const {
	for (const auto compContainer : mComponentsArraysMap) {
		if (!compContainer) {
			continue;
		}
		compContainer->clear();
	}
}

void ComponentManager::destroyComponents(const EntityId entityId) const {
	for (const auto compContainer : mComponentsArraysMap) {
		if (!compContainer) {
			continue;
		}
		compContainer->destroySector(entityId);
	}
}
