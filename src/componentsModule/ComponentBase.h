#pragma once
#include "ecss/Types.h"


namespace ecss {
	/**
	 * @brief Carries the id of the entity a component belongs to.
	 *
	 * Deliberately has no virtual destructor. Nothing in the engine ever holds or deletes a
	 * component through a ComponentInterface*, so the vtable bought nothing -- and it cost
	 * every derived component its triviality, which costs its whole array the memmove paths
	 * in ecss (SectorLayoutMeta::initData). Keep it that way: a virtual function here is paid
	 * for by every component that inherits it.
	 */
	class ComponentInterface {
	public:
		ComponentInterface(SectorId ownerId) : mOwnerId(ownerId) {};

		inline SectorId getEntityId() const { return mOwnerId; };
		inline void setOwnerId(SectorId ownerId) { mOwnerId = ownerId; };
	public:
		SectorId mOwnerId = INVALID_ID; //this variable will be set by components manager or component array after placing into entity sector
	};
}
