#pragma once
#include <mutex>
#include <queue>
#include <typeindex>
#include <unordered_map>

#include "Types.h"
#include "base/EntityBase.h"
#include "memory/ComponentsArray.h"


namespace ECS {
	template <typename T, typename ...ComponentTypes>
	class ComponentsSelector;

	class ComponentManager final {
	public:
		template <class T>
		Memory::ComponentsArray* getComponentContainer();

		ComponentManager();
		~ComponentManager();
		void clearComponents() const;

		template <class T>
		T* getComponent(const EntityHandle& entity) {
			if (!entity) {
				return nullptr;
			}

			return getComponent<T>(entity.getID());
		}

		template <class T, class ...Args>
		T* addComponent(const EntityHandle& entity, Args&&... args) {
			if (!entity) {
				return nullptr;
			}

			return addComponent<T>(entity.getID(), std::forward<Args>(args)...);
		}

		template <class T>
		T* getComponent(const EntityId entityId);

		template <class T, class ...Args>
		T* addComponent(const EntityId entityId, Args&&... args);

		template <class T>
		void moveComponentToEntity(const EntityId entityId, T* component);

		template <class T>
		void copyComponentToEntity(const EntityId entityId, T* component);

		template <class T>
		void removeComponent(const EntityId entityId);

		void destroyComponents(const EntityId entityId) const;

/*this function allows to init container which stores multiple components in one memory sector
 
 0x..[sector info]
 0x..[component 1]
 0x..[component 2]
 0x..[    ...    ]
 0x..[component maxN]
 
  maxN = 32
 
  should be called before any getContainer calls
*/
		template<typename... Components> 
		void createComponentsContainer(); 

/*
this function creates an object with selected components, which provided ability to iterate through entities like it is the container of tuple<component1,component2,component3>
first component type in template is the "main" one, because components stores in separate containers, the first component parent container chosen for iterating

component1Cont		component2Cont		component3Cont
0 [	data	]	[	data	]	[	data	]
- [	null	]	[	data	]	[	data	]
1 [	data	]	[	data	]	[	data	]
2 [	data	]	[	data	]	[	null	]
- [	null	]	[	null	]	[	data	]
3 [	data	]	[	null	]	[	data	]

it will iterate through first 0,1,2,3... container elements

ATTENTION

if componentContainer has multiple components in it, it will iterate through sectors, and may return nullptr for "main" component type
so better, if you want to merge multiple types in one sector, always create all components for sector
*/
		template<typename... Components>
		ComponentsSelector<Components...> createView() { return ComponentsSelector<Components...>(this); }
	private:
		std::vector<Memory::ComponentsArray*> mComponentsArraysMap;
	};

	template <class T>
	Memory::ComponentsArray* ComponentManager::getComponentContainer() {
		if (mComponentsArraysMap.empty()) {
			return nullptr;
		}

		const size_t componentTypeID = T::STATIC_COMPONENT_TYPE_ID;

		if (!mComponentsArraysMap[componentTypeID]) {
			createComponentsContainer<T>();
		}

		return mComponentsArraysMap[componentTypeID];
	}

	template <class T, class ... Args>
	T* ComponentManager::addComponent(const EntityId entityId, Args&&... args) {
		if (auto comp = getComponent<T>(entityId)) {
			return comp;
		}

		auto comp = new(getComponentContainer<T>()->acquireSector(T::STATIC_COMPONENT_TYPE_ID, entityId))T(std::forward<Args>(args)...);
		comp->mOwnerId = entityId;

		return static_cast<T*>(comp);
	}

	//you can create component somewhere in another thread and move it into container here
	template <class T>
	void ComponentManager::moveComponentToEntity(const EntityId entityId, T* component) {
		getComponentContainer<T>()->moveToSector<T>(entityId, component);
	}

	template <class T>
	void ComponentManager::copyComponentToEntity(const EntityId entityId, T* component) {
		getComponentContainer<T>()->copyToSector<T>(entityId, component);
	}

	template <class T>
	void ComponentManager::removeComponent(const EntityId entityId) {
		getComponentContainer<T>()->destroyObject(T::STATIC_COMPONENT_TYPE_ID, entityId);
	}

	template <typename ... Components>
	void ComponentManager::createComponentsContainer() {
		bool added = false;
		((added |= mComponentsArraysMap[Components::STATIC_COMPONENT_TYPE_ID] != nullptr), ...);
		if (added) {
			assert(false);
			return;
		}

		auto container = new Memory::ComponentsArrayInitializer<Components...>();

		((mComponentsArraysMap[Components::STATIC_COMPONENT_TYPE_ID] = container),...);
	}

	template <class T>
	T* ComponentManager::getComponent(const EntityId entityId) {
		return getComponentContainer<T>()->getComponent<T>(entityId);
	}

	template <typename T, typename ...ComponentTypes>
	class ComponentsSelector {
		ComponentManager* mManager;
		Memory::ComponentsArray* mMainContainer;

	public:
		class Iterator {
			using ComponentsIt = Memory::ComponentsArray::Iterator<T>;
			ComponentsIt mIt;
			ComponentManager* mManager;

		public:
			Iterator(ComponentsIt listIt, ComponentManager* manager) : mIt(listIt), mManager(manager) {}

			std::tuple<T&, ComponentTypes&...> operator*() {
				return std::tie(**mIt, *mManager->getComponent<ComponentTypes>(mIt.getSectorId())...);
			}

			Iterator& operator++() {
				++mIt;
				return *this;
			}

			bool operator!=(const Iterator& other) const {
				return mIt != other.mIt;
			}
		};

		size_t size() const { return mMainContainer->size(); }

		std::tuple<T&, ComponentTypes&...> operator[](size_t i) const {
			auto it = Iterator(mMainContainer->begin<T>() + i, mManager);
			return *it;
		}

		ComponentsSelector(ComponentManager* manager) : mManager(manager), mMainContainer(manager->getComponentContainer<T>()) {}

		Iterator begin() {
			return { mMainContainer->begin<T>(), mManager };
		}

		Iterator end() {
			return { mMainContainer->end<T>(), mManager };
		}
	};
}


