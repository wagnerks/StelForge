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
		T* getComponent(const EntityHandle& entity) {
			return entity ? getComponentContainer<T>()->getComponent<T>(entity.getID()) : nullptr;
		}

		template <class T, class ...Args>
		T* addComponent(const EntityHandle& entity, Args&&... args) {
			if (auto comp = getComponent<T>(entity)) {
				return comp;
			}

			auto container = getComponentContainer<T>();
			auto comp = new(container->acquireSector(container->template getTypeIdx<T>(), entity.getID()))T(std::forward<Args>(args)...);
			comp->mOwnerId = entity.getID();

			return static_cast<T*>(comp);
		}

		//you can create component somewhere in another thread and move it into container here
		template <class T>
		void moveComponentToEntity(const EntityHandle& entity, T* component) {
			getComponentContainer<T>()->moveToSector<T>(entity.getID(), component);
		}

		template <class T>
		void copyComponentToEntity(const EntityHandle& entity, T* component) {
			getComponentContainer<T>()->copyToSector<T>(entity.getID(), component);
		}

		template <class T>
		void removeComponent(const EntityHandle& entity) {
			getComponentContainer<T>()->destroyObject(Memory::ReflectionHelper::getTypeId<T>(), entity.getID());
		}

		template <class T>
		void removeComponent(const std::vector<EntityId>& entities) {
			getComponentContainer<T>()->destroyObjects(Memory::ReflectionHelper::getTypeId<T>(), entities);
		}

		void destroyComponents(const EntityHandle& entityId) const;

/*this function allows to init container which stores multiple components in one memory sector
 
 0x..[sector info]
 0x..[component 1]
 0x..[component 2]
 0x..[    ...    ]
 0x..[component maxN]
 
  maxN = 10
 
  should be called before any getContainer calls
*/
		template<typename... Components> 
		void initCustomComponentsContainer() {
			bool added = false;

			((added |= prepareForContainer<Components>()), ...);
			if (added) {
				assert(false);
				return;
			}

			auto container = new Memory::ComponentsArrayInitializer<Components...>();

			((mComponentsArraysMap[Memory::ReflectionHelper::getTypeId<Components>()] = container), ...);
		}

		template <typename T>
		bool prepareForContainer() {
			auto type = Memory::ReflectionHelper::getTypeId<T>();
			if (mComponentsArraysMap.size() <= type) {
				mComponentsArraysMap.resize(type + 1, nullptr);
				return false;
			}
			
			return mComponentsArraysMap[type];
		}

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

		template <class T>
		Memory::ComponentsArray* getComponentContainer(bool skipCheck = false) {
			if (!skipCheck) {
				auto compId = Memory::ReflectionHelper::getTypeId<T>();
				if (mComponentsArraysMap.size() <= compId) {
					mComponentsArraysMap.resize(compId + 1);
				}

				if (mComponentsArraysMap[Memory::ReflectionHelper::getTypeId<T>()] == nullptr) {
					initCustomComponentsContainer<T>();
				}
			}

			return mComponentsArraysMap[Memory::ReflectionHelper::getTypeId<T>()];
		}

	private:
		std::vector<Memory::ComponentsArray*> mComponentsArraysMap;

		std::vector<EntityId> mEntities;
		std::set<EntityId> mFreeEntities;

		EntityId getNewId();
	};
	
	template <typename T, typename ...ComponentTypes>
	class ComponentsArrayHandle {
	public:
		explicit ComponentsArrayHandle(Registry* manager) : mMainContainer(manager->getComponentContainer<T>()) {
			size_t i = 0;
			((mArrays[i++] = manager->getComponentContainer<ComponentTypes>()), ...);
			std::reverse(mArrays.begin(), mArrays.end());
		}

		inline size_t size() const { return mMainContainer->size(); }
		inline bool empty() const { return !size(); }

		inline std::tuple<T&, ComponentTypes&...> operator[](size_t i) const { return *Iterator(mMainContainer->begin() + i, mArrays); }

		class Iterator {
			using ComponentsIt = Memory::ComponentsArray::Iterator;

		public:
			inline Iterator(const ComponentsIt& iterator, const std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)>& arrays) : mIt(std::move(iterator)), mArrays(arrays) {
				size_t i = 0;
				((mHasType[i++] = mIt.hasType<std::remove_const_t<ComponentTypes>>()), ...);
				std::reverse(mHasType.begin(), mHasType.end());

				mMainIndex = mIt.getTypeIdx<T>();
				std::reverse(mArrays.begin(), mArrays.end());

				i = 0;
				((i++,mTypeIndexes[i-1] = mArrays[i-1]->template getTypeIdx<ComponentTypes>()), ...);

				std::reverse(mArrays.begin(), mArrays.end());
				std::reverse(mTypeIndexes.begin(), mTypeIndexes.end());
			}

			template<typename ComponentType>
			inline ComponentType* getComponent(const EntityId sectorId) {
				return mHasType[mIdx] ? mIt.getTyped<ComponentType>(mTypeIndexes[mIdx++]) : mArrays[mIdx]->template getComponentImpl<ComponentType>(sectorId, mTypeIndexes[mIdx++]);
			}
			
			inline std::tuple<size_t, T&, ComponentTypes&...> operator*() {
				return std::forward_as_tuple((*mIt)->id, *(mIt.getTyped<T>(mMainIndex)), *getComponent<ComponentTypes>((*mIt)->id)...);
			}

			inline Iterator& operator++() {	return ++mIt, mIdx = 0, *this; }
			inline bool operator!=(const Iterator& other) const { return mIt != other.mIt; }

		private:
			ComponentsIt mIt;
			std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)> mArrays;
			std::array<bool, sizeof...(ComponentTypes)> mHasType;
			std::array<uint8_t, sizeof...(ComponentTypes)> mTypeIndexes;
			size_t mIdx = 0;

			uint8_t mMainIndex = 0;
		};


		inline Iterator begin() { return { mMainContainer->begin(), mArrays }; }
		inline Iterator end() {	return { mMainContainer->end(), mArrays }; }

	private:
		Memory::ComponentsArray* mMainContainer;
		std::array<Memory::ComponentsArray*, sizeof...(ComponentTypes)> mArrays;
	};
}