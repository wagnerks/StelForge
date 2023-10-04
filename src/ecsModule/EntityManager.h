#pragma once

#include <bitset>
#include <list>

#include "Types.h"
#include "memory/ECSMemoryStack.h"
#include "memory/settings.h"

namespace ECS {
	class EntityComponentSystem;
	class EntityHandle;

	class EntityManager final : Memory::ECSMemoryUser {
	public:
		EntityManager(Memory::ECSMemoryStack* memoryManager, EntityComponentSystem* ecs);
		~EntityManager() override;

		EntityHandle* createEntity(EntityId id = INVALID_ID);
		EntityHandle* getEntity(EntityId entityId) const;

		void destroyEntity(EntityId entityId);
		void destroyEntities();

		std::list<EntityHandle*> entities;
		
	private:
		EntityComponentSystem* mEcsController = nullptr;

		void insertNewEntity(EntityHandle* entity, EntityId id);
		EntityId getNewId();
		std::bitset<MAX_ENTITIES> mEntitiesMap;
		std::bitset<MAX_ENTITIES> mEntitiesDeletingMap;

		std::deque<EntityId> mFreeEntities;
		std::vector<EntityId> mEntitiesToDelete;

		void* mEntitiesBegin = nullptr;
		
		size_t mEntitySize = 0;

		bool mIsTerminating = false;
	};
}
