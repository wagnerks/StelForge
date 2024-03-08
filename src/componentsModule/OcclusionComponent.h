#pragma once
#include "glWrapper/Query.h"

namespace SFE::ComponentsModule {
	struct OcclusionComponent {
		OcclusionComponent() {}

		~OcclusionComponent() {
			delete query;
		}
		GLW::Query<GLW::QueryType::SAMPLES_PASSED>* query = nullptr;

		bool occluded = false;

		std::vector<FrustumModule::AABB> occluderAABB; //occluder is smaller bounding volume  representing object shape which can occlude other objects, it is should be the shape equal or smaller then object itself, (for tree it is only the tree tunk itself)
		std::vector<FrustumModule::AABB> occludeeAABB; //occludee is bigger bounding volume which can be occluded, is should be the shape equal or bigger then object itslef( for tree it is the tunk and leafs)
	};
}
