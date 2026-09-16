#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include "mathModule/Forward.h"

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

		static Plane fromClip(const Math::Vec4& p) {
			const float mag = Math::length(Math::Vec3{ p });
			if (mag < 1e-8f) {
				return {};
			}
			return Plane{ Math::Vec4{ p.x / mag, p.y / mag, p.z / mag, p.w / mag } };
		}

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

	// One bit per face, in this order.
	inline constexpr std::array<Plane Frustum::*, 6> kFrustumFaces {
		&Frustum::leftFace, &Frustum::rightFace, &Frustum::topFace, &Frustum::bottomFace, &Frustum::nearFace, &Frustum::farFace
	};
	inline constexpr uint8_t kAllFrustumFaces = 0b111111;

	// Tests the box against the faces whose bits are set and clears the bits of faces it lies entirely inside.
	// False as soon as the box is outside one face. Children of a box inherit the cleared bits: a box entirely
	// inside a face cannot contain anything outside it, so a walk down a tree tests each face only while it cuts.
	inline bool cullFrustumFaces(const Frustum& frustum, const Math::Vec3& center, const Math::Vec3& extents, uint8_t& faces) {
		for (size_t i = 0; i < kFrustumFaces.size(); ++i) {
			const auto bit = static_cast<uint8_t>(1u << i);
			if (!(faces & bit)) {
				continue;
			}

			const Plane& plane = frustum.*kFrustumFaces[i];
			// projection radius of the box onto the plane normal, see https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plan.html
			const float radius = extents.x * std::abs(plane.normal.x) + extents.y * std::abs(plane.normal.y) + extents.z * std::abs(plane.normal.z);
			const float distance = plane.getSignedDistanceToPlan(center);
			if (distance < -radius) {
				return false;
			}
			if (distance >= radius) {
				faces &= static_cast<uint8_t>(~bit);
			}
		}
		return true;
	}

	struct AABB {
		AABB() = default;
		AABB(const Math::Vec3& min, const Math::Vec3& max) : center((max + min) * 0.5f) {
			const auto e = (max - min) * 0.5f;
			extents = { std::abs(e.x), std::abs(e.y), std::abs(e.z) };
		}
		AABB(const Math::Vec3& inCenter, float iI, float iJ, float iK) : center(inCenter), extents({ iI, iJ, iK }){}

		AABB transformedBy(const Math::Mat4& m) const {
			const Math::Vec3 r{ m[0] * extents.x };
			const Math::Vec3 u{ m[1] * extents.y };
			const Math::Vec3 f{ m[2] * extents.z };
			AABB out;
			out.center = m * center;
			out.extents = {
				std::abs(r.x) + std::abs(u.x) + std::abs(f.x),
				std::abs(r.y) + std::abs(u.y) + std::abs(f.y),
				std::abs(r.z) + std::abs(u.z) + std::abs(f.z)
			};
			return out;
		}

		static bool isOnFrustum(const Frustum& camFrustum, const Math::Vec3& center, const Math::Vec3& extents) {
			uint8_t faces = kAllFrustumFaces;
			return cullFrustumFaces(camFrustum, center, extents, faces);
		}

		Math::Vec3 center{ 0.f, 0.f, 0.f };
		Math::Vec3 extents{};
	};

	// cube given by its centre and half size
	struct SquareAABB {
		static bool isOnFrustum(const Frustum& camFrustum, const Math::Vec3& center, float halfSize) {
			return AABB::isOnFrustum(camFrustum, center, Math::Vec3{ halfSize });
		}
	};

	//https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
	// Rows combined for NO clip depth [-1, 1]; a _ZO projection would need near = row 2 alone.
	inline Frustum createFrustum(const Math::Mat4& projView) {
		Frustum frustum;

		const auto row = [&projView](size_t r) {
			return Math::Vec4{ projView[0][r], projView[1][r], projView[2][r], projView[3][r] };
		};
		const Math::Vec4 row0 = row(0);
		const Math::Vec4 row1 = row(1);
		const Math::Vec4 row2 = row(2);
		const Math::Vec4 row3 = row(3);

		frustum.leftFace = Plane::fromClip(row3 + row0);
		frustum.rightFace = Plane::fromClip(row3 - row0);
		frustum.bottomFace = Plane::fromClip(row3 + row1);
		frustum.topFace = Plane::fromClip(row3 - row1);
		frustum.nearFace = Plane::fromClip(row3 + row2);
		frustum.farFace = Plane::fromClip(row3 - row2);

		static constexpr std::array<Math::Vec4, 8> ndcCorners = {
			Math::Vec4(-1.0f, -1.0f, -1.0f, 1.0f), // Near bottom left
			Math::Vec4(1.0f, -1.0f, -1.0f, 1.0f), // Near bottom right
			Math::Vec4(-1.0f,  1.0f, -1.0f, 1.0f), // Near top left
			Math::Vec4(1.0f,  1.0f, -1.0f, 1.0f), // Near top right
			Math::Vec4(-1.0f, -1.0f,  1.0f, 1.0f), // Far bottom left
			Math::Vec4(1.0f, -1.0f,  1.0f, 1.0f), // Far bottom right
			Math::Vec4(-1.0f,  1.0f,  1.0f, 1.0f), // Far top left
			Math::Vec4(1.0f,  1.0f,  1.0f, 1.0f)  // Far top right
		};

		const Math::Mat4 invViewProj = Math::inverse(projView);

		frustum.minPoint = Math::Vec3{ std::numeric_limits<float>::max() };
		frustum.maxPoint = Math::Vec3{ std::numeric_limits<float>::lowest() };

		for (const Math::Vec4& ndcCorner : ndcCorners) {
			const Math::Vec4 worldCoord = invViewProj * ndcCorner;
			const Math::Vec3 point = worldCoord.xyz / worldCoord.w; // Homogeneous divide

			frustum.minPoint = Math::componentMin(frustum.minPoint, point);
			frustum.maxPoint = Math::componentMax(frustum.maxPoint, point);
		}

		return frustum;
	}
}
