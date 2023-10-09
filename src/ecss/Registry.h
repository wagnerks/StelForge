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

		EntityHandle takeEntity();
		EntityHandle getEntity(EntityId entityId) const;

		void destroyEntity(const EntityHandle& entityId);

		const std::vector<EntityId>& getAllEntities();
	private:
		template <class T>
		Memory::ComponentsArray* getComponentContainer(bool skipCheck = false) {
			if (mComponentsArraysMap.empty()) {
				return nullptr;
			}

			const size_t componentTypeID = T::STATIC_COMPONENT_TYPE_ID;
			if (!skipCheck) {
				if (mComponentsArraysMap[componentTypeID] == mDummy) {
					initCustomComponentsContainer<T>();
				}
			}

			return mComponentsArraysMap[componentTypeID];
		}

		std::vector<Memory::ComponentsArray*> mComponentsArraysMap;

		std::vector<EntityId> mEntities;
		std::set<EntityId> mFreeEntities;

		Memory::ComponentsArray* mDummy;

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
		((added |= mComponentsArraysMap[Components::STATIC_COMPONENT_TYPE_ID] != mDummy), ...);
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

		auto cont = getComponentContainer<T>();
		if (cont->entitiesCapacity() <= entity.getID()) {
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

		auto comp = new(getComponentContainer<T>()->acquireSector(T::STATIC_COMPONENT_SECTOR_IDX, entity.getID()))T(std::forward<Args>(args)...);
		comp->mOwnerId = entity.getID();

		return static_cast<T*>(comp);
	}

	template <typename T, typename ...ComponentTypes>
	class ComponentsArrayHandle {
		Memory::ComponentsArray* mMainContainer;

		std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)> mArrays;
	public:
		class Iterator {
			using ComponentsIt = Memory::ComponentsArray::Iterator<T>;
			ComponentsIt mIt;
			std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)> mArrays;
			std::array<bool, sizeof...(ComponentTypes)> mIsTyped;
			size_t mIdx = 0;

		public:
			inline Iterator(ComponentsIt listIt, std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)> arrays) : mIt(listIt), mArrays(arrays) {
				size_t i = 0;
				((mIsTyped[i++] = mIt.template isType< std::remove_const_t<ComponentTypes>>()), ...);

				std::reverse(mIsTyped.begin(), mIsTyped.end());
			}

			template<typename ComponentType>
			inline ComponentType* getComponent(const EntityId& sectorId) {
				return mIsTyped[mIdx] ? mIdx++, mIt.template getTyped<ComponentType>() : mArrays[mIdx++]->template getComponent<ComponentType>(sectorId);
			}
			
			inline std::tuple<T&, ComponentTypes&...> operator*() {
				return std::forward_as_tuple(*(mIt.template getTyped<T>()), *getComponent<ComponentTypes>(*static_cast<EntityId*>(*(mIt.mIt)))...);
			}

			inline Iterator& operator++() {
				++mIt, mIdx = 0; return *this;
			}

			inline bool operator!=(const Iterator& other) const {
				return mIt != other.mIt;
			}
		};

		size_t size() const { return mMainContainer->size(); }
		bool empty() const { return !size(); }

		std::tuple<T&, ComponentTypes&...> operator[](size_t i) const {
			return *Iterator(mMainContainer->begin<T>() + i, mArrays);
		}

		ComponentsArrayHandle(Registry* manager) : mMainContainer(manager->getComponentContainer<T>()) {
			size_t i = 0;
			((mArrays[i++] = manager->getComponentContainer<ComponentTypes>(true)), ...);

			std::reverse(mArrays.begin(), mArrays.end());
		}

		Iterator begin() {
			return { mMainContainer->begin<T>(), mArrays };
		}

		Iterator end() {
			return { mMainContainer->end<T>(), mArrays };
		}
	};
}