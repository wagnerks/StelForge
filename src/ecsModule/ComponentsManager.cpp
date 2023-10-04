#include "ComponentsManager.h"

#include "base/ComponentBase.h"

using namespace ECS;

ComponentManager::ComponentManager(Memory::ECSMemoryStack* memoryManager) : ECSMemoryUser(memoryManager) {
	mComponentsArraysMap.resize(StaticTypeCounter<ComponentInterface>::getCount(), nullptr);
}

ComponentManager::~ComponentManager() {
	std::unordered_map<void*, bool> deleted;

	for (const auto container : mComponentsArraysMap) {
		if (!container || deleted[container]) {//skip not created and containers of multiple components
			continue;
		}

		container->~ComponentsArray();
		deleted[container] = true;
	}
}

void ComponentManager::clearComponents() {
	for (const auto compContainer : mComponentsArraysMap) {
		if (!compContainer) {
			continue;
		}
		compContainer->clearAllSectors();
	}

	mIsTerminating = true;
}

void ComponentManager::destroyComponents(const EntityId entityId) const {
	if (mIsTerminating) {
		return;
	}

	for (const auto compContainer : mComponentsArraysMap) {
		if (!compContainer) {
			continue;
		}
		compContainer->destroySector(entityId);
	}
}
