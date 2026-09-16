#pragma once

#include <algorithm>
#include <optional>

#include "Forward.h"
#include "Utils.h"

namespace SFE::Math {
	struct Ray {
		Vec3 a;
		Vec3 direction;
	};

	// Where the line through the ray crosses the plane, on either side of the origin; nothing when they are parallel.
	[[nodiscard]] inline std::optional<Vec3> intersectLinePlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal) {
		const float denominator = dot(planeNormal, ray.direction);
		if (std::abs(denominator) <= std::numeric_limits<float>::epsilon()) {
			return std::nullopt;
		}
		return ray.a + ray.direction * (dot(planeNormal, planePoint - ray.a) / denominator);
	}

	// World-space ray from the camera through a pixel. Screen origin is top-left; the ray starts on the near plane.
	[[nodiscard]] inline Ray screenToWorldRay(const Mat4& projView, const Vec2& screenCoords, const Vec2& screenSize, const Vec3& cameraPos) {
		const float ndcX = (2.f * screenCoords.x) / screenSize.x - 1.f;
		const float ndcY = 1.f - (2.f * screenCoords.y) / screenSize.y;
		const Vec4 nearPoint = inverse(projView) * Vec4{ ndcX, ndcY, -1.f, 1.f };
		const Vec3 worldPoint = nearPoint.xyz / nearPoint.w;
		return { worldPoint, normalize(worldPoint - cameraPos) };
	}

	[[nodiscard]] inline float calcAngleOnCircle(float chordLength, float radius) {
		return 2.f * std::asin(chordLength / (2.f * radius));
	}

	// Angle from a to b in [0, 2pi), counter-clockwise when looking down -Z (by the sign of cross(a, b).z).
	[[nodiscard]] inline float calcAngleBetweenVectors(const Vec3& a, const Vec3& b) {
		const float magnitudes = length(a) * length(b);
		if (magnitudes <= 0.f) {
			return 0.f;
		}

		const float angle = std::acos(std::clamp(dot(a, b) / magnitudes, -1.f, 1.f));
		return cross(a, b).z < 0.f ? twoPi<float>() - angle : angle;
	}

	//    a*
	//    /
	//   /
	//b *------* c
	[[nodiscard]] inline float calcAngleBetweenVectors(const Vec3& a, const Vec3& b, const Vec3& c) {
		return calcAngleBetweenVectors(b - a, b - c);
	}
}
