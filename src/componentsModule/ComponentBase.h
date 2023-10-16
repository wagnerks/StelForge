#pragma once
#include "ecss/Types.h"


namespace ecss {
	class ComponentInterface {
	public:
		ComponentInterface(EntityId ownerId) : mOwnerId(ownerId) {};
		virtual ~ComponentInterface() = default;

		inline EntityId getEntityId() const { return mOwnerId; };
		inline void setOwnerId(EntityId ownerId) { mOwnerId = ownerId; };
	public:
		EntityId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};
}
