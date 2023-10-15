#pragma once

#include "Types.h"

namespace ecss {
	class EntityHandle {
	public:
		operator bool() const { return mId != INVALID_ID; }
		EntityHandle(EntityId entityID = INVALID_ID) : mId(entityID) {}
		EntityId getID() const { return mId; }

	private:
		EntityId mId = INVALID_ID;
	};
}