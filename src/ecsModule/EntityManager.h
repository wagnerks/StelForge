#pragma once

#include <deque>
#include <set>

#include "Types.h"

namespace ECS {
	class EntityComponentSystem;
	class EntityHandle;

	class EntityManager final {
	public:
		EntityManager(EntityComponentSystem* ecs);

		EntityHandle takeEntity(EntityId id = INVALID_ID);
		EntityHandle getEntity(EntityId entityId) const;

		void destroyEntity(EntityId entityId);
		void destroyEntities();

		const std::set<EntityId>& getAllEntities();
	private:
		std::set<EntityId> mEntities;

		EntityComponentSystem* mEcsController = nullptr;

		EntityId getNewId();

		std::deque<EntityId> mFreeEntities;
		std::set<EntityId> mEntitiesToDelete;
	};
}
