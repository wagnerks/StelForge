#include "BoundingVolume.h"

namespace SFE::FrustumModule  {
	AABB Frustum::generateAABB() const {
		return AABB(minPoint, maxPoint);
	}
}
