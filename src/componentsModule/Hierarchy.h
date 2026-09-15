#pragma once

#include <vector>

#include "ecss/Types.h"

namespace SFE {
	/**
	 * @brief Parent and child entities, as an ecss relation instead of a component.
	 *
	 * The edge points from the child to its parent, and an entity has at most one of them. A relation is not stored
	 * in either entity's sector, so the components stay trivially copyable, and the registry drops every edge of an
	 * entity when it is destroyed -- which a component holding ids has to do by hand from its destructor.
	 *
	 * Reads take the relation's shared lock for as long as the view lives, so these helpers copy what they need and
	 * let the view go. Never change the hierarchy while iterating it.
	 */
	struct ChildOf {};

	namespace Hierarchy {
		// INVALID_ID when the entity has no parent.
		ecss::EntityId parent(ecss::EntityId entity);
		std::vector<ecss::EntityId> children(ecss::EntityId entity);
		// Children, their children, and so on.
		std::vector<ecss::EntityId> descendants(ecss::EntityId entity);
		bool hasChildren(ecss::EntityId entity);

		// Replaces the entity's parent; INVALID_ID detaches it. An entity cannot be its own parent.
		void setParent(ecss::EntityId child, ecss::EntityId parent);
		void detach(ecss::EntityId child);
	}
}
