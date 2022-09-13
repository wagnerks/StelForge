#pragma once
#include <array>

#include "Camera.h"
#include "componentsModule/TransformComponent.h"
#include "modelModule/Model.h"

namespace GameEngine::FrustumModule {
	struct Plane
	{
		glm::vec3 normal	= { 0.f, 1.f, 0.f }; // unit vector
		float     distance  = 0.f;					      // Distance with origin

		Plane() = default;

		Plane(const glm::vec3& p1, const glm::vec3& norm)
			: normal(glm::normalize(norm)),
			distance(glm::dot(normal, p1))
		{}
		Plane(const glm::vec4& p1) : normal(glm::vec3(p1)), distance(p1.w)
		{}
		float getSignedDistanceToPlan(const glm::vec3& point) const
		{
			auto d = glm::dot(normal, point) + distance;
			return d;
		}
	};

	struct Frustum {
		Plane topFace;
		Plane bottomFace;

		Plane rightFace;
		Plane leftFace;

		Plane farFace;
		Plane nearFace;
	};

	struct BoundingVolume
	{
		virtual bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const = 0;

		virtual bool isOnOrForwardPlan(const Plane& plan) const = 0;

		bool isOnFrustum(const Frustum& camFrustum) const
		{
			return (isOnOrForwardPlan(camFrustum.leftFace) &&
				isOnOrForwardPlan(camFrustum.rightFace) &&
				isOnOrForwardPlan(camFrustum.topFace) &&
				isOnOrForwardPlan(camFrustum.bottomFace) &&
				isOnOrForwardPlan(camFrustum.nearFace) &&
				isOnOrForwardPlan(camFrustum.farFace));
		};
	};

	struct Sphere : public BoundingVolume
	{
		glm::vec3 center{ 0.f, 0.f, 0.f };
		float radius{ 0.f };
		Sphere() = default;
		Sphere(const glm::vec3& inCenter, float inRadius)
			: BoundingVolume{}, center{ inCenter }, radius{ inRadius }
		{}

		bool isOnOrForwardPlan(const Plane& plan) const final
		{
			return plan.getSignedDistanceToPlan(center) > -radius;
		}

		bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const final
		{
			//Get global scale thanks to our transform
			const glm::vec3& globalScale = transform.getScale(true);

			//Get our global center with process it with the global model matrix of our transform
			const glm::vec3& globalCenter = transform.getPos(true);

			//To wrap correctly our shape, we need the maximum scale scalar.
			const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);

			//Max scale is assuming for the diameter. So, we need the half to apply it to our radius
			Sphere globalSphere(globalCenter, radius * (maxScale * 0.5f));

			//Check Firstly the result that have the most chance to faillure to avoid to call all functions.
			return (globalSphere.isOnOrForwardPlan(camFrustum.leftFace) &&
				globalSphere.isOnOrForwardPlan(camFrustum.rightFace) &&
				globalSphere.isOnOrForwardPlan(camFrustum.farFace) &&
				globalSphere.isOnOrForwardPlan(camFrustum.nearFace) &&
				globalSphere.isOnOrForwardPlan(camFrustum.topFace) &&
				globalSphere.isOnOrForwardPlan(camFrustum.bottomFace));
		};
	};
	
	struct SquareAABB : public BoundingVolume
	{
		glm::vec3 center{ 0.f, 0.f, 0.f };
		float extent{ 0.f };
	
		SquareAABB(const glm::vec3& inCenter, float inExtent)
			: BoundingVolume{}, center{ inCenter }, extent{ inExtent }
		{}
	
		bool isOnOrForwardPlan(const Plane& plan) const final
		{
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = extent * (std::abs(plan.normal.x) + std::abs(plan.normal.y) + std::abs(plan.normal.z));
			return -r <= plan.getSignedDistanceToPlan(center);
		}
	
		bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const final
		{
			//Get global scale thanks to our transform
			const glm::vec3 globalCenter{ transform.getTransform() * glm::vec4(center, 1.f) };
	
			// Scaled orientation
			const glm::vec3 right = transform.getTransform()[0] * extent; //right
			const glm::vec3 up = transform.getTransform()[1] * extent; //up
			const glm::vec3 forward = -transform.getTransform()[2] * extent; //forward
	
			const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));
	
			const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));
	
			const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));
	
			const SquareAABB globalAABB(globalCenter, std::max(std::max(newIi, newIj), newIk));
	
			return (globalAABB.isOnOrForwardPlan(camFrustum.leftFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.rightFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.topFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.bottomFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.nearFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.farFace));
		};
	};
	struct AABB : public BoundingVolume
	{
		glm::vec3 center{ 0.f, 0.f, 0.f };
		glm::vec3 extents{ 0.f, 0.f, 0.f };

		AABB(const glm::vec3& min, const glm::vec3& max)
			: BoundingVolume{}, center{ (max + min) * 0.5f }, extents{ max.x - center.x, max.y - center.y, max.z - center.z }
		{}

		AABB(const glm::vec3& inCenter, float iI, float iJ, float iK)
			: BoundingVolume{}, center{ inCenter }, extents{ iI, iJ, iK }
		{}

		std::array<glm::vec3, 8> getVertice() const
		{
			std::array<glm::vec3, 8> vertice;
			vertice[0] = { center.x - extents.x, center.y - extents.y, center.z - extents.z };
			vertice[1] = { center.x + extents.x, center.y - extents.y, center.z - extents.z };
			vertice[2] = { center.x - extents.x, center.y + extents.y, center.z - extents.z };
			vertice[3] = { center.x + extents.x, center.y + extents.y, center.z - extents.z };
			vertice[4] = { center.x - extents.x, center.y - extents.y, center.z + extents.z };
			vertice[5] = { center.x + extents.x, center.y - extents.y, center.z + extents.z };
			vertice[6] = { center.x - extents.x, center.y + extents.y, center.z + extents.z };
			vertice[7] = { center.x + extents.x, center.y + extents.y, center.z + extents.z };
			return vertice;
		}

		//see https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plan.html
		bool isOnOrForwardPlan(const Plane& plan) const final
		{
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = extents.x * std::abs(plan.normal.x) + extents.y * std::abs(plan.normal.y) +
				extents.z * std::abs(plan.normal.z);

			return -r <= plan.getSignedDistanceToPlan(center);
		}

		bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const final
		{
			//Get global scale thanks to our transform
			const glm::vec3 globalCenter{ transform.getTransform() * glm::vec4(center, 1.f) };

			// Scaled orientation
			const glm::vec3 right = transform.getTransform()[0] * extents.x; //right
			const glm::vec3 up = transform.getTransform()[1] * extents.y; //up
			const glm::vec3 forward = -transform.getTransform()[2] * extents.z; //forward

			const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

			const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

			const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
				std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

			const AABB globalAABB(globalCenter, newIi, newIj, newIk);

			return (globalAABB.isOnOrForwardPlan(camFrustum.leftFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.rightFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.topFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.bottomFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.nearFace) &&
				globalAABB.isOnOrForwardPlan(camFrustum.farFace));
		};
	};


	inline AABB generateAABB(const ModelModule::Mesh& mesh)
	{
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

	inline Sphere generateSphereBV(const GameEngine::ModelModule::Mesh& mesh)
	{
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

	inline void normalizePlane(glm::vec4& planeVec) {
		auto mag = glm::sqrt(planeVec.x * planeVec.x + planeVec.y * planeVec.y + planeVec.z * planeVec.z );
		planeVec.x /= mag; 
		planeVec.y /= mag; 
		planeVec.z /= mag; 
		planeVec.w /= mag;
	}

	//https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
	inline Frustum createFrustum(const glm::mat4& projView) {

		Frustum frustum;
		glm::vec4 left;
		glm::vec4 right;

		glm::vec4 btm;
		glm::vec4 top;

		glm::vec4 near;
		glm::vec4 far;

		left.x = projView[0][3] + projView[0][0];
		left.y = projView[1][3] + projView[1][0];
		left.z = projView[2][3] + projView[2][0];
		left.w = projView[3][3] + projView[3][0];
		normalizePlane(left);

		right.x = projView[0][3] - projView[0][0];
		right.y = projView[1][3] - projView[1][0];
		right.z = projView[2][3] - projView[2][0];
		right.w = projView[3][3] - projView[3][0];
		normalizePlane(right);

		btm.x = projView[0][3] + projView[0][1];
		btm.y = projView[1][3] + projView[1][1];
		btm.z = projView[2][3] + projView[2][1];
		btm.w = projView[3][3] + projView[3][1];
		normalizePlane(btm);

		top.x = projView[0][3] - projView[0][1];
		top.y = projView[1][3] - projView[1][1];
		top.z = projView[2][3] - projView[2][1];
		top.w = projView[3][3] - projView[3][1];
		normalizePlane(top);

		near.x = projView[0][3] + projView[0][2];
		near.y = projView[1][3] + projView[1][2];
		near.z = projView[2][3] + projView[2][2];
		near.w = projView[3][3] + projView[3][2];
		normalizePlane(near);

		far.x = projView[0][3] - projView[0][2];
		far.y = projView[1][3] - projView[1][2];
		far.z = projView[2][3] - projView[2][2];
		far.w = projView[3][3] - projView[3][2];
		normalizePlane(far);

		frustum.leftFace = { left };
		frustum.rightFace = { right };

		frustum.bottomFace = { btm };
		frustum.topFace = { top };

		frustum.nearFace = { near };
		frustum.farFace = { far };

		return frustum;
	}
}
