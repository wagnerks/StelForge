#pragma once
#include <mutex>
#include <queue>
#include <typeindex>
#include <unordered_map>

#include "base/EntityBase.h"
#include "memory/ComponentsArray.h"
#include "memory/settings.h"
#include "memory/ECSMemoryStack.h"


namespace ECS {
	class ComponentManager : Memory::ECSMemoryUser {
	public:
		template <typename T, typename ...ComponentTypes>
		class ComponentsIterator;

		template <class T>
		Memory::ComponentsArray* getComponentContainer();

		ComponentManager(Memory::ECSMemoryStack* memoryManager);
		~ComponentManager() override;
		void clearComponents();

		template <class T>
		T* getComponent(const EntityHandle* entity) {
			if (!entity) {
				return nullptr;
			}

			return getComponent<T>(entity->getEntityID());
		}
		template <class T, class ...Args>
		T* addComponent(const EntityHandle* entity, Args&&... args) {
			if (!entity) {
				return nullptr;
			}

			return addComponent<T>(entity->getEntityID());
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

		template<typename... Components> 
		void createComponentsContainer();

		template<typename... Components>
		ComponentsIterator<Components...> processComponents() { return ComponentsIterator<Components...>(this); }
	private:
		std::vector<Memory::ComponentsArray*> mComponentsArraysMap;
		bool mIsTerminating = false;

	private:
		template <typename T, typename ...ComponentTypes>
		class ComponentsIterator {
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

			ComponentsIterator(ComponentManager* manager) : mManager(manager), mMainContainer(manager->getComponentContainer<T>()) {}

			Iterator begin() {
				return { mMainContainer->begin<T>(), mManager };
			}

			Iterator end() {
				return { mMainContainer->end<T>(), mManager };
			}
		};
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

		//mtx.lock();//this shit can lock main thread if acquireComponentId waiting synchronization
		auto comp = new(getComponentContainer<T>()->acquireSector(T::STATIC_COMPONENT_TYPE_ID, entityId))T(std::forward<Args>(args)...);
		comp->mOwnerId = entityId;

		//mtx.unlock();

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

		auto container = new (allocate(sizeof(Memory::ComponentsArrayInitializer<Components...>) + alignof(Memory::ComponentsArrayInitializer<Components...>)))Memory::ComponentsArrayInitializer<Components...>(MAX_ENTITIES, mStack);

		((mComponentsArraysMap[Components::STATIC_COMPONENT_TYPE_ID] = container),...);
	}

	template <class T>
	T* ComponentManager::getComponent(const EntityId entityId) {
		return getComponentContainer<T>()->getComponent<T>(entityId);
	}
}


