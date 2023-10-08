#include "BoundingVolume.h"
#include <assetsModule/modelModule/Mesh.h>

Engine::FrustumModule::AABB Engine::FrustumModule::generateAABB(const AssetsModule::Mesh& mesh) {
	glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::min());


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

Engine::FrustumModule::Sphere Engine::FrustumModule::generateSphereBV(const AssetsModule::Mesh& mesh) {
	glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::min());

	for (auto& vertex : mesh.mData.mVertices)
	{
		minAABB.x = std::min(minAABB.x, vertex.mPosition.x);
		minAABB.y = std::min(minAABB.y, vertex.mPosition.y);
		minAABB.z = std::min(minAABB.z, vertex.mPosition.z);

		maxAABB.x = std::max(maxAABB.x, vertex.mPosition.x);
		maxAABB.y = std::max(maxAABB.y, vertex.mPosition.y);
		maxAABB.z = std::max(maxAABB.z, vertex.mPosition.z);
	}


	return Sphere((maxAABB + minAABB) * 0.5f, glm::length(minAABB - maxAABB));
}
