#include "ComponentsManager.h"

using namespace ecsModule;

#define ENITY_LUT_GROW 1024

ComponentManager::ComponentManager(GameEngine::MemoryModule::MemoryManager* memoryManager) : GlobalMemoryUser(memoryManager) {
	GameEngine::LogsModule::Logger::LOG_INFO("Initialize ComponentManager");

	const size_t NUM_COMPONENTS{FamilySize<ComponentInterface>::Get()};

	entityComponentMap.resize(ENITY_LUT_GROW);
	for (auto i = 0; i < ENITY_LUT_GROW; ++i) {
		entityComponentMap[i].resize(NUM_COMPONENTS, ecsModule::INVALID_ID);
	}
}

ComponentManager::~ComponentManager() {
	for (auto& cc : componentContainerRegistry) {
		if (cc.second) {
			GameEngine::LogsModule::Logger::LOG_INFO("Releasing remaining entities of type '%s' ...", cc.second->getComponentContainerTypeName());
		}
		
		delete cc.second;
	}

	GameEngine::LogsModule::Logger::LOG_INFO("Release ComponentManager");
}

void ComponentManager::removeAllComponents(const size_t entityId) {
	static const size_t NUM_COMPONENTS = entityComponentMap[0].size();

	for (size_t componentType = 0; componentType < NUM_COMPONENTS; ++componentType) {
		const size_t componentId = entityComponentMap[entityId][componentType];
		if (componentId == ecsModule::INVALID_ID)
			continue;

		auto component = componentLookupTable[componentId];
		if (component != nullptr) {
			auto it = componentContainerRegistry.find(componentType);
			if (it != componentContainerRegistry.end()) {
				it->second->destroyElement(component);
			}
			else {
				assert(false && "Trying to release a component that wasn't created by ComponentManager!");
			}

			releaseEntityComponent(entityId, componentId, componentType);
		}
	}
}

size_t ComponentManager::acquireComponentId(ComponentInterface* component) {
	size_t i = 0;
	for (; i < componentLookupTable.size(); ++i) {
		if (componentLookupTable[i] == nullptr) {
			componentLookupTable[i] = component;
			return i;
		}
	}

	componentLookupTable.resize(componentLookupTable.size() + ENITY_LUT_GROW, nullptr);

	componentLookupTable[i] = component;
	return i;
}

void ComponentManager::releaseComponentId(size_t id) {
	if (id == ecsModule::INVALID_ID || id >= componentLookupTable.size()) {
		assert(false && "Invalid component id");
		return;
	}
	
	componentLookupTable[id] = nullptr;
}

void ComponentManager::mapEntityComponent(size_t entityId, size_t componentId, size_t componentSize) {
	static const size_t NUM_COMPONENTS{FamilySize<ComponentInterface>::Get()};
	if (NUM_COMPONENTS == 0) {
		assert(false && "no components but try to allocate");
		return;
	}

	if (entityComponentMap.size() <= entityId) {
		const size_t oldSize = entityComponentMap.size();
		const size_t newSize = oldSize + ENITY_LUT_GROW;

		entityComponentMap.resize(newSize);

		for (auto i = oldSize; i < newSize; ++i) {
			entityComponentMap[i].resize(NUM_COMPONENTS, ecsModule::INVALID_ID);
		}
	}

	entityComponentMap[entityId][componentSize] = componentId;
}

void ComponentManager::releaseEntityComponent(size_t entityId, size_t componentId, size_t componentType) {
	if (entityComponentMap[entityId][componentType] != componentId) {
		assert(false && "FATAL: Entity Component ID mapping corruption!");
		return;
	}

	entityComponentMap[entityId][componentType] = ecsModule::INVALID_ID;
	releaseComponentId(componentId);
}
