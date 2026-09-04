#pragma once

#include <vector>
#include <assetsModule/modelModule/BoundingVolume.h>
#include "ecss/Types.h"

namespace SFE::ComponentsModule {
	struct OcclusionComponent {
		bool occluded = false;

		std::vector<FrustumModule::AABB> occluderAABB; //occluder is smaller bounding volume  representing object shape which can occlude other objects, it is should be the shape equal or smaller then object itself, (for tree it is only the tree tunk itself)
		std::vector<FrustumModule::AABB> occludeeAABB; //occludee is bigger bounding volume which can be occluded, is should be the shape equal or bigger then object itslef( for tree it is the tunk and leafs)
	};

	struct OccludedComponent {
		bool occluded = false;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::OcclusionComponent> : std::true_type {};
