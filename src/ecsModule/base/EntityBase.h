#pragma once

#include "../memory/settings.h"
#include "string_view"

namespace ECS {
	class EntityHandle final {
		friend class EntityManager;
	public:
		EntityHandle(EntityId entityID) : mId(entityID) {}

		EntityId getEntityID() const { return mId; }

		void setStringId(std::string_view id) { mStringId = id; }
		std::string_view getStringId() { return mStringId; }
	private:
		~EntityHandle() { }

		const EntityId mId = INVALID_ID;
		std::string mStringId;
	};
}