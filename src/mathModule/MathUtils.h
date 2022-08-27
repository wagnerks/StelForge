#pragma once

#include "glm.hpp"

namespace GameEngine::Math {
	inline static float fastSqrt(float val){
		return glm::inversesqrt(val);
	}

	inline static float distanceSqr(const glm::vec3& p1, const glm::vec3& p2){
		auto x = p2.x - p1.x;
		auto y = p2.y - p1.y;
		auto z = p2.y - p1.y;

		return x*x + y*y + z*z;
	}

	inline static float distance(const glm::vec3& p1, const glm::vec3& p2){
		auto lengthSqr = distanceSqr(p1, p2);
		return fastSqrt(lengthSqr);
	}
}