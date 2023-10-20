#pragma once
#include "ecss/Types.h"


namespace ecss {
	class ComponentInterface {
	public:
		ComponentInterface(SectorId ownerId) : mOwnerId(ownerId) {};
		virtual ~ComponentInterface() = default;

		inline SectorId getEntityId() const { return mOwnerId; };
		inline void setOwnerId(SectorId ownerId) { mOwnerId = ownerId; };
	public:
		SectorId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};
}
