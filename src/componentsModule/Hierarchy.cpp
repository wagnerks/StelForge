#include "Hierarchy.h"

#include <algorithm>

#include "core/ECSHandler.h"

namespace SFE::Hierarchy {
	ecss::EntityId parent(const ecss::EntityId entity) {
		if (entity == ecss::INVALID_ID) {
			return ecss::INVALID_ID;
		}
		for (const auto id : ECSHandler::registry().targets<ChildOf>(entity)) {
			return id;
		}
		return ecss::INVALID_ID;
	}

	std::vector<ecss::EntityId> children(const ecss::EntityId entity) {
		std::vector<ecss::EntityId> result;
		if (entity == ecss::INVALID_ID) {
			return result;
		}
		for (const auto id : ECSHandler::registry().sources<ChildOf>(entity)) {
			result.push_back(id);
		}
		// The relation lists the newest edge first.
		std::reverse(result.begin(), result.end());
		return result;
	}

	std::vector<ecss::EntityId> descendants(const ecss::EntityId entity) {
		auto result = children(entity);
		// Index based: the loop appends to the same vector it walks.
		for (size_t i = 0; i < result.size(); ++i) {
			for (const auto id : ECSHandler::registry().sources<ChildOf>(result[i])) {
				result.push_back(id);
			}
		}
		return result;
	}

	bool hasChildren(const ecss::EntityId entity) {
		if (entity == ecss::INVALID_ID) {
			return false;
		}
		for (const auto id : ECSHandler::registry().sources<ChildOf>(entity)) {
			(void)id;
			return true;
		}
		return false;
	}

	void setParent(const ecss::EntityId child, const ecss::EntityId parentEntity) {
		if (child == ecss::INVALID_ID) {
			return;
		}
		if (parentEntity == ecss::INVALID_ID) {
			detach(child);
			return;
		}
		// parent() copies the id and closes its view, so no lock is held when the edge changes below.
		for (auto node = parentEntity; node != ecss::INVALID_ID; node = parent(node)) {
			if (node == child) {
				return;
			}
		}
		// One call, one lock: detaching and relating separately could leave two parents if two threads race.
		ECSHandler::registry().relateOnly<ChildOf>(child, parentEntity);
	}

	void detach(const ecss::EntityId child) {
		if (child != ecss::INVALID_ID) {
			ECSHandler::registry().removeRelationsFrom<ChildOf>(child);
		}
	}
}
