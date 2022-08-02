#pragma once
#include <array>

#include "Camera.h"
#include "componentsModule/TransformComponent.h"
#include "modelModule/Model.h"

namespace GameEngine::FrustumModule {
	struct Plan
	{
		glm::vec3 normal = { 0.f, 1.f, 0.f }; // unit vector
		float     distance = 0.f;        // Distance with origin

		Plan() = default;

		Plan(const glm::vec3& p1, const glm::vec3& norm)
			: normal(glm::normalize(norm)),
			distance(glm::dot(normal, p1))
		{}

		float getSignedDistanceToPlan(const glm::vec3& point) const
		{
			return glm::dot(normal, point) - distance;
		}
	};

	struct Frustum {
		Plan topFace;
		Plan bottomFace;

		Plan rightFace;
		Plan leftFace;

		Plan farFace;
		Plan nearFace;
	};

	struct BoundingVolume
	{
		virtual bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const = 0;

		virtual bool isOnOrForwardPlan(const Plan& plan) const = 0;

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

		bool isOnOrForwardPlan(const Plan& plan) const final
		{
			return plan.getSignedDistanceToPlan(center) > -radius;
		}

		bool isOnFrustum(const Frustum& camFrustum, const TransformComponent& transform) const final
		{
			//Get global scale thanks to our transform
			const glm::vec3 globalScale = transform.getScale();

			//Get our global center with process it with the global model matrix of our transform
			const glm::vec3 globalCenter{ transform.getTransform() * glm::vec4(center, 1.f) };

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
	
		bool isOnOrForwardPlan(const Plan& plan) const final
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
		bool isOnOrForwardPlan(const Plan& plan) const final
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

		
		for (auto&& vertex : mesh.vertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.Position.x);
			minAABB.y = std::min(minAABB.y, vertex.Position.y);
			minAABB.z = std::min(minAABB.z, vertex.Position.z);

			maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
			maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
			maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
		}
		
		return AABB(minAABB, maxAABB);
	}

	inline Sphere generateSphereBV(const GameEngine::ModelModule::Mesh& mesh)
	{
		glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::min());

		for (auto& vertex : mesh.vertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.Position.x);
			minAABB.y = std::min(minAABB.y, vertex.Position.y);
			minAABB.z = std::min(minAABB.z, vertex.Position.z);

			maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
			maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
			maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
		}
		

		return Sphere((maxAABB + minAABB) * 0.5f, glm::length(minAABB - maxAABB));
	}

	inline Frustum createPerspectiveProjectionFrustum(TransformComponent* viewTransform, float aspect, float fovY, float zNear, float zFar) {
		if (!viewTransform) {
			return {};
		}

		Frustum frustum;
		const float halfVSide = zFar * tanf(fovY * 0.5f);
		const float halfHSide = halfVSide * aspect;
		
		viewTransform->reloadTransform();

		const glm::vec3 frontMultFar = zFar * viewTransform->getForward();

		auto pos = viewTransform->getPos();

		frustum.nearFace = { pos + zNear * viewTransform->getForward(), viewTransform->getForward() };
		frustum.farFace = { pos + frontMultFar, -viewTransform->getForward() };
		frustum.rightFace = { pos, glm::cross(viewTransform->getUp(), frontMultFar + viewTransform->getRight() * halfHSide) };
		frustum.leftFace = { pos, glm::cross(frontMultFar - viewTransform->getRight() * halfHSide, viewTransform->getUp()) };
		frustum.topFace = { pos, glm::cross(viewTransform->getRight(), frontMultFar - viewTransform->getUp() * halfVSide) };
		frustum.bottomFace = { pos, glm::cross(frontMultFar + viewTransform->getUp() * halfVSide, viewTransform->getRight()) };
		return frustum;
	}
}
