#include "BoundingVolume.h"
#include <assetsModule/modelModule/Mesh.h>

namespace SFE::FrustumModule  {
	AABB Frustum::generateAABB() const {
		return AABB(minPoint, maxPoint);
	}

	//AABB SFE::FrustumModule::generateAABB(const std::vector<SFE::Vertex3D>& vertices) {
	//	auto minAABB = Math::Vec3(std::numeric_limits<float>::max());
	//	auto maxAABB = Math::Vec3(std::numeric_limits<float>::min());


	//	for (auto&& vertex : vertices) {
	//		minAABB.x = std::min(minAABB.x, vertex.position.x);
	//		minAABB.y = std::min(minAABB.y, vertex.position.y);
	//		minAABB.z = std::min(minAABB.z, vertex.position.z);

	//		maxAABB.x = std::max(maxAABB.x, vertex.position.x);
	//		maxAABB.y = std::max(maxAABB.y, vertex.position.y);
	//		maxAABB.z = std::max(maxAABB.z, vertex.position.z);
	//	}

	//	return AABB(minAABB, maxAABB);
	//}

	//Sphere SFE::FrustumModule::generateSphereBV(const SFE::Mesh<Vertex3D>& mesh) {
	//	Math::Vec3 minAABB = Math::Vec3(std::numeric_limits<float>::max());
	//	Math::Vec3 maxAABB = Math::Vec3(std::numeric_limits<float>::min());

	//	for (auto& vertex : mesh.vertices)
	//	{
	//		minAABB.x = std::min(minAABB.x, vertex.position.x);
	//		minAABB.y = std::min(minAABB.y, vertex.position.y);
	//		minAABB.z = std::min(minAABB.z, vertex.position.z);

	//		maxAABB.x = std::max(maxAABB.x, vertex.position.x);
	//		maxAABB.y = std::max(maxAABB.y, vertex.position.y);
	//		maxAABB.z = std::max(maxAABB.z, vertex.position.z);
	//	}


	//	return Sphere((maxAABB + minAABB) * 0.5f, Math::distance(maxAABB, minAABB));
	//}

}

