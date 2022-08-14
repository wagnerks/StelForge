#pragma once

#include <unordered_map>
#include "Container.h"

namespace ecsModule {
	class EntityInterface;

	class EntityManager : GameEngine::MemoryModule::GlobalMemoryUser {
	public:
		EntityManager(GameEngine::MemoryModule::MemoryManager* memoryManager);
		~EntityManager() override;

		template <class T, class... ARGS>
		T* createEntity(ARGS&&... args) {
			void* pObjectMemory = getEntityContainer<T>()->createObject();

			auto entityId = acquireEntityId(static_cast<T*>(pObjectMemory));
			auto entity = new(pObjectMemory)T(entityId, std::forward<ARGS>(args)...);

			return entity;
		}

		EntityInterface* getEntity(size_t entityId) const;

		template<class entityType>
		inline EntityInterface* getEntity(size_t entityId) {
			return static_cast<entityType*>(mEntities[entityId]);
		}

		const std::vector<EntityInterface*>& getAllEntities() {
			return mEntities;
		}

		void destroyEntity(size_t entityId);
		void destroyEntities();
	private:
		template <class T>
		inline Container<T>* getEntityContainer() {
			const auto entityTypeID = T::STATIC_ENTITY_TYPE_ID;

			auto it = mEntityContainers.find(entityTypeID);
			Container<T>* ec = nullptr;

			if (it == mEntityContainers.end()) {
				ec = new Container<T>(std::type_index(typeid(this)).hash_code(), globalMemoryManager);
				mEntityContainers[entityTypeID] = ec;
			}
			else
				ec = static_cast<Container<T>*>(it->second);

			assert(ec != nullptr && "Failed to create EntityContainer<T>!");
			return ec;
		}


		size_t acquireEntityId(EntityInterface* entity);
		void releaseEntityId(size_t id);

		std::vector<EntityInterface*> mEntities; //pos == id
		std::unordered_map<size_t, ContainerInterface*> mEntityContainers;
		std::vector<size_t> mEntitiesToDelete;
	};
}
