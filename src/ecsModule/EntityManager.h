#pragma once

#include <unordered_map>
#include "Container.h"

#define ENITY_T_CHUNK_SIZE					512

namespace ecsModule {
	class EntityInterface;

	class EntityManager : GameEngine::MemoryModule::GlobalMemoryUser {
		std::vector<EntityInterface*> mEntities; //pos == id

		std::unordered_map<size_t, ContainerInterface*> mEntityContainers;

		std::vector<size_t> mEntitiesToDelete;
	private:
		template <class T>
		inline Container<T>* getEntityContainer() {
			auto EID = T::STATIC_ENTITY_TYPE_ID;

			auto it = mEntityContainers.find(EID);
			Container<T>* ec = nullptr;

			if (it == mEntityContainers.end()) {
				ec = new Container<T>(std::type_index(typeid(this)).hash_code(), globalMemoryManager);
				mEntityContainers[EID] = ec;
			}
			else
				ec = static_cast<Container<T>*>(it->second);

			assert(ec != nullptr && "Failed to create EntityContainer<T>!");
			return ec;
		}


		size_t acquireEntityId(EntityInterface* entity);


		void releaseEntityId(size_t id);

	public:
		EntityManager(GameEngine::MemoryModule::MemoryManager* memoryManager);
		~EntityManager() override;


		template <class T, class... ARGS>
		size_t createEntity(ARGS&&... args) {
			void* pObjectMemory = getEntityContainer<T>()->createObject();

			auto entityId = acquireEntityId(static_cast<T*>(pObjectMemory));
			auto entity = new(pObjectMemory)T(entityId, std::forward<ARGS>(args)...);

			return entityId;
		}


		void destroyEntity(size_t entityId) {
			mEntitiesToDelete.emplace_back(entityId);
		}

		inline EntityInterface* getEntity(size_t entityId) const {
			if (entityId >= mEntities.size()) {
				return nullptr;
			}
			return mEntities[entityId];
		}

		template<class entityType>
		inline EntityInterface* getEntity(size_t entityId) {
			return static_cast<entityType*>(mEntities[entityId]);
		}

		void destroyEntities();
	};
}
