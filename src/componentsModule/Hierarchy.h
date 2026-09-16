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
		// In the order they were attached, so saving and loading a scene keeps the order.
		std::vector<ecss::EntityId> children(ecss::EntityId entity);
		// Children, their children, and so on.
		std::vector<ecss::EntityId> descendants(ecss::EntityId entity);
		bool hasChildren(ecss::EntityId entity);

		// Replaces the entity's parent; INVALID_ID detaches it. Does nothing if the new parent is the entity itself
		// or one of its descendants: that would make a cycle, and walking the tree would never end.
		void setParent(ecss::EntityId child, ecss::EntityId parent);
		void detach(ecss::EntityId child);
	}
}
