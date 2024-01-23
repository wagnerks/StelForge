#include "BoundingVolume.h"
#include <assetsModule/modelModule/Mesh.h>

namespace SFE::FrustumModule  {
	AABB Frustum::generateAABB() {
		return AABB(minPoint, maxPoint);
	}

	AABB SFE::FrustumModule::generateAABB(const AssetsModule::Mesh& mesh) {
		Math::Vec3 minAABB = Math::Vec3(std::numeric_limits<float>::max());
		Math::Vec3 maxAABB = Math::Vec3(std::numeric_limits<float>::min());


		for (auto&& vertex : mesh.mData.mVertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.mPosition.x);
			minAABB.y = std::min(minAABB.y, vertex.mPosition.y);
			minAABB.z = std::min(minAABB.z, vertex.mPosition.z);

			maxAABB.x = std::max(maxAABB.x, vertex.mPosition.x);
			maxAABB.y = std::max(maxAABB.y, vertex.mPosition.y);
			maxAABB.z = std::max(maxAABB.z, vertex.mPosition.z);
		}

		return AABB(minAABB, maxAABB);
	}

	Sphere SFE::FrustumModule::generateSphereBV(const AssetsModule::Mesh& mesh) {
		Math::Vec3 minAABB = Math::Vec3(std::numeric_limits<float>::max());
		Math::Vec3 maxAABB = Math::Vec3(std::numeric_limits<float>::min());

		for (auto& vertex : mesh.mData.mVertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.mPosition.x);
			minAABB.y = std::min(minAABB.y, vertex.mPosition.y);
			minAABB.z = std::min(minAABB.z, vertex.mPosition.z);

			maxAABB.x = std::max(maxAABB.x, vertex.mPosition.x);
			maxAABB.y = std::max(maxAABB.y, vertex.mPosition.y);
			maxAABB.z = std::max(maxAABB.z, vertex.mPosition.z);
		}


		return Sphere((maxAABB + minAABB) * 0.5f, Math::distance(maxAABB, minAABB));
	}

}

