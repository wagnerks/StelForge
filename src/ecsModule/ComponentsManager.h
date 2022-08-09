#pragma once
#include <typeindex>
#include <unordered_map>

#include "ComponentBase.h"
#include "Container.h"
#include "helper.h"
#include "memoryModule/MemoryChunkAllocator.h"



namespace ecsModule {
	class ComponentManager : GameEngine::MemoryModule::GlobalMemoryUser {
	public:
		template <class T>
		Container<T>* getComponentContainer();
		
		ComponentManager(GameEngine::MemoryModule::MemoryManager* memoryManager);
		~ComponentManager() override;

		template <class T>
		T* getComponent(const size_t entityId);

		template <class T, class ...Args>
		T* addComponent(const size_t entityId,  Args&&... args);

		template <class T>
		void removeComponent(const size_t entityId);

		void removeAllComponents(const size_t entityId);
	private:
		size_t acquireComponentId(ComponentInterface* component);
		void releaseComponentId(size_t id);

		void mapEntityComponent(size_t entityId, size_t componentId, size_t componentSize);
		void releaseEntityComponent(size_t entityId, size_t componentId, size_t componentType);

		std::unordered_map<size_t, ContainerInterface*> componentContainerRegistry;
		std::vector<ComponentInterface*> componentLookupTable; //pos == componentID
		std::vector<std::vector<size_t>> entityComponentMap;
	};

	template <class T>
	Container<T>* ComponentManager::getComponentContainer() {
		const size_t componentTypeID = T::STATIC_COMPONENT_TYPE_ID;

		Container<T>* compContainer = nullptr;
		if (const auto it = componentContainerRegistry.find(componentTypeID); it == componentContainerRegistry.end()) {
			compContainer = new Container<T>(std::type_index(typeid(this)).hash_code(), globalMemoryManager);
			componentContainerRegistry[componentTypeID] = compContainer;
		}
		else {
			compContainer = static_cast<Container<T>*>(it->second);
		}

		GameEngine::LogsModule::Logger::LOG_FATAL(compContainer, "Failed to create ComponentContainer<T>!");

		return compContainer;
	}

	template <class T, class ... Args>
	T* ComponentManager::addComponent(const size_t entityId,  Args&&... args) {
		void* pObjectMemory = getComponentContainer<T>()->createObject();

		auto componentId = acquireComponentId(static_cast<T*>(pObjectMemory));

		static_cast<T*>(pObjectMemory)->setId(componentId);
		ComponentInterface* component = new(pObjectMemory)T(std::forward<Args>(args)...);

		component->setOwnerId(entityId);
			
		mapEntityComponent(entityId, componentId, T::STATIC_COMPONENT_TYPE_ID);

		return static_cast<T*>(component);
	}

	template <class T>
	void ComponentManager::removeComponent(const size_t entityId) {
		if (entityId >= entityComponentMap.size()) {
			return;
		}

		const auto CTID = T::STATIC_COMPONENT_TYPE_ID;
		if (CTID >= entityComponentMap[entityId].size()) {
			return;
		}

		const auto componentId = entityComponentMap[entityId][CTID];
		if (componentId == ecsModule::INVALID_ID) {
			return;//component not exists
		}

		ComponentInterface* component = componentLookupTable[componentId];
		if (!component ) {
			GameEngine::LogsModule::Logger::LOG_FATAL(false, "Trying to remove a component which is not used by this entity!");
			return;
		}

		getComponentContainer<T>()->destroyObject(component);

		releaseEntityComponent(entityId, componentId, CTID);
	}

	template <class T>
	T* ComponentManager::getComponent(const size_t entityId) {
		if (entityId >= entityComponentMap.size()) {
			return nullptr;
		}

		const auto CTID = T::STATIC_COMPONENT_TYPE_ID;
		if (CTID >= entityComponentMap[entityId].size()) {
			return nullptr;
		}

		const auto componentId = entityComponentMap[entityId][CTID];

		
		if (componentId == ecsModule::INVALID_ID) {
			return nullptr;
		}

		return static_cast<T*>(componentLookupTable[componentId]);
	}
}


