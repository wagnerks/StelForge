#pragma once

#include <vector>

#include "mathModule/Forward.h"

namespace AssetsModule {
	class Mesh;
}

namespace SFE::FrustumModule {

	struct AABB;

	struct Plane {
		Math::Vec3 normal = { 0.f, 1.f, 0.f }; // unit vector
		float     distance = 0.f;					   // Distance with origin

		Plane() = default;

		Plane(const Math::Vec3& p1, const Math::Vec3& norm)
			: normal(Math::normalize(norm))
			, distance(Math::dot(normal, p1)) {}

		Plane(const Math::Vec4& p1) : normal(Math::Vec3(p1)), distance(p1.w) {}

		float getSignedDistanceToPlan(const Math::Vec3& point) const {
			return Math::dot(normal, point) + distance;
		}
	};

	struct Frustum {
		Plane topFace;
		Plane bottomFace;

		Plane rightFace;
		Plane leftFace;

		Plane farFace;
		Plane nearFace;

		Math::Vec3 minPoint;
		Math::Vec3 maxPoint;
		AABB generateAABB() const;
	};
	//todo add frustum bounding volume
	struct BoundingVolume {
		BoundingVolume() = default;
		BoundingVolume(const Math::Vec3& center) : center(center){}

		virtual ~BoundingVolume() = default;

		virtual bool isOnFrustum(const Frustum& camFrustum, const Math::Mat4& transform) const = 0;
		virtual bool isOnOrForwardPlan(const Plane& plan) const = 0;

		virtual bool isOnFrustum(const Frustum& camFrustum) const {
			return isOnOrForwardPlan(camFrustum.leftFace) 
				&& isOnOrForwardPlan(camFrustum.rightFace) 
				&& isOnOrForwardPlan(camFrustum.topFace) 
				&& isOnOrForwardPlan(camFrustum.bottomFace) 
				&& isOnOrForwardPlan(camFrustum.nearFace) 
				&& isOnOrForwardPlan(camFrustum.farFace);
		}


		Math::Vec3 calculateGlobalCenter(const Math::Mat4& transform) const {
			return calculateGlobalCenter(center, transform);
		}

		static Math::Vec3 calculateGlobalCenter(const Math::Vec3& center, const Math::Mat4& transform) {
			return { transform * Math::Vec4(center, 1.f) };
		}



	public:
		Math::Vec3 center{ 0.f, 0.f, 0.f };
	};

	struct Sphere : BoundingVolume {
		using BoundingVolume::isOnFrustum;

		Sphere() = default;
		Sphere(const Math::Vec3& center, float radius) : BoundingVolume{ center }, extents(radius) {}

		template<typename Vec3>
		inline static bool isOnOrForwardPlan(const Plane& plan, const Vec3& center, float radius) {
			return plan.getSignedDistanceToPlan(center) > -radius;
		}

		template<typename Vec3>
		inline static bool isOnFrustum(const Frustum& camFrustum, const Vec3& center, float radius) {
			return isOnOrForwardPlan(camFrustum.leftFace, center, radius) &&
				isOnOrForwardPlan(camFrustum.rightFace, center, radius) &&
				isOnOrForwardPlan(camFrustum.farFace, center, radius) &&
				isOnOrForwardPlan(camFrustum.nearFace, center, radius) &&
				isOnOrForwardPlan(camFrustum.topFace, center, radius) &&
				isOnOrForwardPlan(camFrustum.bottomFace, center, radius);
		}

		inline bool isOnOrForwardPlan(const Plane& plan) const final {
			return isOnOrForwardPlan(plan, center, extents);
		}

		inline bool isOnFrustum(const Frustum& camFrustum, const Math::Mat4& t/*transform*/) const final {
			//Get global scale thanks to our transform
			const Math::Vec3& globalScale = {
				sqrt(t[0][0] * t[0][0] + t[0][1] * t[0][1] + t[0][2] * t[0][2]),
				sqrt(t[1][0] * t[1][0] + t[1][1] * t[1][1] + t[1][2] * t[1][2]),
				sqrt(t[2][0] * t[2][0] + t[2][1] * t[2][1] + t[2][2] * t[2][2])
			};

			//To wrap correctly our shape, we need the maximum scale scalar.
			const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);

			//Max scale is assuming for the diameter. So, we need the half to apply it to our radius
			return isOnFrustum(camFrustum, calculateGlobalCenter(t), extents * (maxScale * 0.5f));
		}

	public:
		float extents{};
	};

	struct SquareAABB : BoundingVolume {
		using BoundingVolume::isOnFrustum;

		SquareAABB() = default;
		SquareAABB(const Math::Vec3& center, float extent) : BoundingVolume{ center }, extents(extent) {}

		template<typename Vec3>
		inline static bool isOnOrForwardPlan(const Plane& plan, const Vec3& center, float extent) {
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = extent * (std::abs(plan.normal.x) + std::abs(plan.normal.y) + std::abs(plan.normal.z));
			return -r <= plan.getSignedDistanceToPlan(center);
		}

		template<typename Vec3>
		inline static bool isOnFrustum(const Frustum& camFrustum, const Vec3& center, float size) {
			return isOnOrForwardPlan(camFrustum.leftFace, center, size)
				&& isOnOrForwardPlan(camFrustum.rightFace, center, size)
				&& isOnOrForwardPlan(camFrustum.topFace, center, size)
				&& isOnOrForwardPlan(camFrustum.bottomFace, center, size)
				&& isOnOrForwardPlan(camFrustum.nearFace, center, size)
				&& isOnOrForwardPlan(camFrustum.farFace, center, size);
		}

		inline bool isOnOrForwardPlan(const Plane& plan) const final {
			return isOnOrForwardPlan(plan, center, extents);
		}

		inline bool isOnFrustum(const Frustum& camFrustum, const Math::Mat4& transform) const final {
			const Math::Vec3 right	 { transform[0] * extents}; //right
			const Math::Vec3 up		 { transform[1] * extents}; //up
			const Math::Vec3 forward {-transform[2] * extents}; //forward

			static constexpr auto I = Math::Vec3{ 1.f, 0.f, 0.f };
			static constexpr auto J = Math::Vec3{ 0.f, 1.f, 0.f };
			static constexpr auto K = Math::Vec3{ 0.f, 0.f, 1.f };
			
			return isOnFrustum(camFrustum, calculateGlobalCenter(transform), std::max(std::max(
				std::abs(Math::dot(I, right)) + std::abs(Math::dot(I, up)) + std::abs(Math::dot(I, forward)),
				std::abs(Math::dot(J, right)) + std::abs(Math::dot(J, up)) + std::abs(Math::dot(J, forward))),
				std::abs(Math::dot(K, right)) + std::abs(Math::dot(K, up)) + std::abs(Math::dot(K, forward))
			));
		};

	public:
		float extents{};
	};


	struct AABB : BoundingVolume {
		using BoundingVolume::isOnFrustum;

		AABB() = default;

		AABB(const Math::Vec3& min, const Math::Vec3& max) : BoundingVolume{ (max + min) * 0.5f}, extents{ max.x - center.x, max.y - center.y, max.z - center.z } {
			;
		}

		AABB(const Math::Vec3& inCenter, float iI, float iJ, float iK) : BoundingVolume{ inCenter }, extents({ iI, iJ, iK }){}
				
		inline bool isOnOrForwardPlan(const Plane& plan) const final {
			return isOnOrForwardPlan(plan, center, extents);
		}

		template<typename Vec3>
		inline static bool isOnOrForwardPlan(const Plane& plan, const Vec3& center, const Vec3& extents) {
			// see https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plan.html
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = extents.x * std::abs(plan.normal.x) + extents.y * std::abs(plan.normal.y) + extents.z * std::abs(plan.normal.z);
			return -r <= plan.getSignedDistanceToPlan(center);
		}

		template<typename Vec3>
		inline static bool isOnFrustum(const Frustum& camFrustum, const Vec3& center, const Vec3& extents) {
			return isOnOrForwardPlan(camFrustum.leftFace,   center, extents)
				&& isOnOrForwardPlan(camFrustum.rightFace,  center, extents)
				&& isOnOrForwardPlan(camFrustum.topFace,    center, extents)
				&& isOnOrForwardPlan(camFrustum.bottomFace, center, extents)
				&& isOnOrForwardPlan(camFrustum.nearFace,   center, extents)
				&& isOnOrForwardPlan(camFrustum.farFace,    center, extents);
		}

		inline bool isOnFrustum(const Frustum& camFrustum, const Math::Mat4& transform) const final {
			const Math::Vec3 right	 { transform[0] * extents.x};
			const Math::Vec3 up		 { transform[1] * extents.y};
			const Math::Vec3 forward {-transform[2] * extents.z};

			static constexpr auto I = Math::Vec3{ 1.f, 0.f, 0.f };
			static constexpr auto J = Math::Vec3{ 0.f, 1.f, 0.f };
			static constexpr auto K = Math::Vec3{ 0.f, 0.f, 1.f };

			return isOnFrustum(camFrustum, calculateGlobalCenter(transform),
				{
					std::abs(Math::dot(I, right)) + std::abs(Math::dot(I, up)) + std::abs(Math::dot(I, forward)),
					std::abs(Math::dot(J, right)) + std::abs(Math::dot(J, up)) + std::abs(Math::dot(J, forward)),
					std::abs(Math::dot(K, right)) + std::abs(Math::dot(K, up)) + std::abs(Math::dot(K, forward))
				}
			);
		}
	public:
		Math::Vec3 extents{};
	};


	inline AABB generateAABB(const AssetsModule::Mesh& mesh);

	inline Sphere generateSphereBV(const AssetsModule::Mesh& mesh);
	
	//https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
	inline Frustum createFrustum(const Math::Mat4& projView) {
		Frustum frustum;

		Math::Vec4 left;
		Math::Vec4 right;
		Math::Vec4 btm;
		Math::Vec4 top;

		Math::Vec4 near;
		Math::Vec4 far;

		left.x = projView[0][3] + projView[0][0];
		left.y = projView[1][3] + projView[1][0];
		left.z = projView[2][3] + projView[2][0];
		left.w = projView[3][3] + projView[3][0];
		left = Math::normalize(left);
		
		right.x = projView[0][3] - projView[0][0];
		right.y = projView[1][3] - projView[1][0];
		right.z = projView[2][3] - projView[2][0];
		right.w = projView[3][3] - projView[3][0];
		right = Math::normalize(right);

		btm.x = projView[0][3] + projView[0][1];
		btm.y = projView[1][3] + projView[1][1];
		btm.z = projView[2][3] + projView[2][1];
		btm.w = projView[3][3] + projView[3][1];
		btm = Math::normalize(btm);

		top.x = projView[0][3] - projView[0][1];
		top.y = projView[1][3] - projView[1][1];
		top.z = projView[2][3] - projView[2][1];
		top.w = projView[3][3] - projView[3][1];
		top = Math::normalize(top);

		near.x = projView[0][3] + projView[0][2];
		near.y = projView[1][3] + projView[1][2];
		near.z = projView[2][3] + projView[2][2];
		near.w = projView[3][3] + projView[3][2];
		near = Math::normalize(near);

		far.x = projView[0][3] - projView[0][2];
		far.y = projView[1][3] - projView[1][2];
		far.z = projView[2][3] - projView[2][2];
		far.w = projView[3][3] - projView[3][2];
		far = Math::normalize(far);

		frustum.leftFace = { left };
		frustum.rightFace = { right };

		frustum.bottomFace = { btm };
		frustum.topFace = { top };

		frustum.nearFace = { near };
		frustum.farFace = { far };




		// NDC coordinates of frustum corners
		static std::vector<Math::Vec4> ndcCorners = {
			Math::Vec4(-1.0f, -1.0f, -1.0f, 1.0f), // Near bottom left
			Math::Vec4(1.0f, -1.0f, -1.0f, 1.0f), // Near bottom right
			Math::Vec4(-1.0f,  1.0f, -1.0f, 1.0f), // Near top left
			Math::Vec4(1.0f,  1.0f, -1.0f, 1.0f), // Near top right
			Math::Vec4(-1.0f, -1.0f,  1.0f, 1.0f), // Far bottom left
			Math::Vec4(1.0f, -1.0f,  1.0f, 1.0f), // Far bottom right
			Math::Vec4(-1.0f,  1.0f,  1.0f, 1.0f), // Far top left
			Math::Vec4(1.0f,  1.0f,  1.0f, 1.0f)  // Far top right
		};

		// Inverse view-projection matrix
		Math::Mat4 invViewProj = Math::inverse(projView);

		frustum.minPoint = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		frustum.maxPoint = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };


		// Transform NDC coordinates to world coordinates
		for (const Math::Vec4& ndcCorner : ndcCorners) {
			Math::Vec4 worldCoord = invViewProj * ndcCorner;
			worldCoord /= worldCoord.w; // Homogeneous divide

			frustum.minPoint.x = std::min(frustum.minPoint.x, worldCoord.x);
			frustum.minPoint.y = std::min(frustum.minPoint.y, worldCoord.y);
			frustum.minPoint.z = std::min(frustum.minPoint.z, worldCoord.z);

			frustum.maxPoint.x = std::max(frustum.maxPoint.x, worldCoord.x);
			frustum.maxPoint.y = std::max(frustum.maxPoint.y, worldCoord.y);
			frustum.maxPoint.z = std::max(frustum.maxPoint.z, worldCoord.z);
		}



		return frustum;
	}
}
