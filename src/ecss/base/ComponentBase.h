#pragma once

#include "../Types.h"

namespace ecss {
	class ComponentInterface {
	public:
		ComponentInterface() = default;
		virtual ~ComponentInterface() = default;

		inline EntityId getEntityId() const { return mOwnerId; };
	public:
		EntityId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};
}
