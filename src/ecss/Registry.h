#pragma once

// ecss - Entity Component System with Sectors
// "Sectors" refers to the logic of storing components.
// Multiple components of different types can be stored in one memory location, which I've named a "sector."

#include <deque>
#include <set>

#include "Types.h"
#include "base/EntityHandle.h"
#include "memory/ComponentsArray.h"


namespace ecss {
	class Registry final {
		template <typename T, typename ...ComponentTypes>
		friend class ComponentsArrayHandle;

		Registry(const Registry& other) = delete;
		Registry(Registry&& other) noexcept = delete;
		Registry& operator=(const Registry& other) = delete;
		Registry& operator=(Registry&& other) noexcept = delete;

	public:
		Registry();
		~Registry();

		void clear();

		template <class T>
		T* getComponent(const EntityHandle& entity);

		template <class T, class ...Args>
		T* addComponent(const EntityHandle& entity, Args&&... args);

		//you can create component somewhere in another thread and move it into container here
		template <class T>
		void moveComponentToEntity(const EntityHandle& entityId, T* component);

		template <class T>
		void copyComponentToEntity(const EntityHandle& entityId, T* component);

		template <class T>
		void removeComponent(const EntityHandle& entityId);

		void destroyComponents(const EntityHandle& entityId) const;

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
		void initCustomComponentsContainer(); 

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
		ComponentsArrayHandle<Components...> getComponentsArray() { return ComponentsArrayHandle<Components...>(this); }

		template <class... Components>
		void reserve(size_t newCapacity) { (getComponentContainer<Components>()->reserve(newCapacity),...); }

		EntityHandle takeEntity(EntityId id = INVALID_ID);
		EntityHandle getEntity(EntityId entityId) const;

		void destroyEntity(const EntityHandle& entityId);

		const std::set<EntityId>& getAllEntities();
	private:
		template <class T>
		Memory::ComponentsArray* getComponentContainer() {
			if (mComponentsArraysMap.empty()) {
				return nullptr;
			}

			const size_t componentTypeID = T::STATIC_COMPONENT_TYPE_ID;

			if (!mComponentsArraysMap[componentTypeID]) {
				initCustomComponentsContainer<T>();
			}

			return mComponentsArraysMap[componentTypeID];
		}

		std::vector<Memory::ComponentsArray*> mComponentsArraysMap;

		std::set<EntityId> mEntities;
		std::deque<EntityId> mFreeEntities;

		EntityId getNewId();
	};

	template <class T>
	void Registry::moveComponentToEntity(const EntityHandle& entity, T* component) {
		getComponentContainer<T>()->moveToSector<T>(entity.getID(), component);
	}

	template <class T>
	void Registry::copyComponentToEntity(const EntityHandle& entity, T* component) {
		getComponentContainer<T>()->copyToSector<T>(entity.getID(), component);
	}

	template <class T>
	void Registry::removeComponent(const EntityHandle& entity) {
		getComponentContainer<T>()->destroyObject(T::STATIC_COMPONENT_TYPE_ID, entity.getID());
	}
	
	template <typename ... Components>
	void Registry::initCustomComponentsContainer() {
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
	T* Registry::getComponent(const EntityHandle& entity) {
		if (!entity) {
			return nullptr;
		}

		return getComponentContainer<T>()->getComponent<T>(entity.getID());
	}

	template <class T, class ... Args>
	T* Registry::addComponent(const EntityHandle& entity, Args&&... args) {
		if (!entity) {
			return nullptr;
		}
		if (auto comp = getComponent<T>(entity)) {
			return comp;
		}

		auto comp = new(getComponentContainer<T>()->acquireSector(T::STATIC_COMPONENT_TYPE_ID, entity.getID()))T(std::forward<Args>(args)...);
		comp->mOwnerId = entity.getID();

		return static_cast<T*>(comp);
	}

	template <typename T, typename ...ComponentTypes>
	class ComponentsArrayHandle {
		
		Registry* mManager;
		Memory::ComponentsArray* mMainContainer;

	public:
		class Iterator {
			using ComponentsIt = Memory::ComponentsArray::Iterator<T>;
			ComponentsIt mIt;
			Registry* mManager;

		public:
			Iterator(ComponentsIt listIt, Registry* manager) : mIt(listIt), mManager(manager) {}

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
		bool empty() const { return !size(); }

		std::tuple<T&, ComponentTypes&...> operator[](size_t i) const {
			auto it = Iterator(mMainContainer->begin<T>() + i, mManager);
			return *it;
		}

		ComponentsArrayHandle(Registry* manager) : mManager(manager), mMainContainer(manager->getComponentContainer<T>()) {}

		Iterator begin() {
			return { mMainContainer->begin<T>(), mManager };
		}

		Iterator end() {
			return { mMainContainer->end<T>(), mManager };
		}
	};
}


