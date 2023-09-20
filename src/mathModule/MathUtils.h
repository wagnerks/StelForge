#pragma once

#include <glm.hpp>

namespace Engine::Math {
	template<typename T>
	struct Vec2 {
		T x = 0.f;
		T y = 0.f;
	};

	template<typename T>
	struct Vec3 {
		T x = 0.f;
		T y = 0.f;
		T z = 0.f;
	};

	template<typename T>
	struct Mat3 {
		Mat3() = default;
		Mat3(Vec3<T> a, Vec3<T> b, Vec3<T> c) :
			x0(a.x), y0(a.y), z0(a.z),
			x1(b.x), y1(b.y), z1(b.z),
			x2(c.x), y2(c.y), z2(c.z)
		{
		}

		T x0 = 0.f;
		T y0 = 0.f;
		T z0 = 0.f;

		T x1 = 0.f;
		T y1 = 0.f;
		T z1 = 0.f;

		T x2 = 0.f;
		T y2 = 0.f;
		T z2 = 0.f;
	};

	inline static float fastSqrt(float val) {
		return glm::inversesqrt(val);
	}

	inline static glm::vec3 pointsToVector(const glm::vec3& p1, const glm::vec3& p2) {
		glm::vec3 res;
		res.x = p2.x - p1.x;
		res.y = p2.y - p1.y;
		res.z = p2.z - p1.z;

		return res;
	}

	inline static float distanceSqr(const glm::vec3& p1, const glm::vec3& p2) {
		auto x = p2.x - p1.x;
		auto y = p2.y - p1.y;
		auto z = p2.y - p1.y;

		return x * x + y * y + z * z;
	}

	inline static float distance(const glm::vec3& p1, const glm::vec3& p2) {
		auto lengthSqr = distanceSqr(p1, p2);
		return fastSqrt(lengthSqr);
	}

	/*inline static glm::mat4x4 matrixMultiplication(const glm::mat4x4& m1, const glm::mat4x4& m2) {
		glm::mat4 res;
		res[0][0] = m1[0][0] * m2[0][0] + m1[0][1] * m2[1][0] + m1[0][2] * m2[2][0] + m1[0][3] * m2[3][0];
		res[0][1] = m1[0][0] * m2[0][1] + m1[0][1] * m2[1][1] + m1[0][2] * m2[2][1] + m1[0][3] * m2[3][1];
		res[0][2] = m1[0][0] * m2[0][2] + m1[0][1] * m2[1][2] + m1[0][2] * m2[2][2] + m1[0][3] * m2[3][2];
		res[0][3] = m1[0][0] * m2[0][3] + m1[0][1] * m2[1][3] + m1[0][2] * m2[2][3] + m1[0][3] * m2[3][3];

		res[1][0] = m1[1][0] * m2[0][0] + m1[1][1] * m2[1][0] + m1[1][2] * m2[2][0] + m1[1][3] * m2[3][0];
		res[1][1] = m1[1][0] * m2[0][1] + m1[1][1] * m2[1][1] + m1[1][2] * m2[2][1] + m1[1][3] * m2[3][1];
		res[1][2] = m1[1][0] * m2[0][2] + m1[1][1] * m2[1][2] + m1[1][2] * m2[2][2] + m1[1][3] * m2[3][2];
		res[1][3] = m1[1][0] * m2[0][3] + m1[1][1] * m2[1][3] + m1[1][2] * m2[2][3] + m1[1][3] * m2[3][3];

		res[2][0] = m1[2][0] * m2[0][0] + m1[2][1] * m2[1][0] + m1[2][2] * m2[2][0] + m1[2][3] * m2[3][0];
		res[2][1] = m1[2][0] * m2[0][1] + m1[2][1] * m2[1][1] + m1[2][2] * m2[2][1] + m1[2][3] * m2[3][1];
		res[2][2] = m1[2][0] * m2[0][2] + m1[2][1] * m2[1][2] + m1[2][2] * m2[2][2] + m1[2][3] * m2[3][2];
		res[2][3] = m1[2][0] * m2[0][3] + m1[2][1] * m2[1][3] + m1[2][2] * m2[2][3] + m1[2][3] * m2[3][3];

		res[3][0] = m1[3][0] * m2[0][0] + m1[3][1] * m2[1][0] + m1[3][2] * m2[2][0] + m1[3][3] * m2[3][0];
		res[3][1] = m1[3][0] * m2[0][1] + m1[3][1] * m2[1][1] + m1[3][2] * m2[2][1] + m1[3][3] * m2[3][1];
		res[3][2] = m1[3][0] * m2[0][2] + m1[3][1] * m2[1][2] + m1[3][2] * m2[2][2] + m1[3][3] * m2[3][2];
		res[3][3] = m1[3][0] * m2[0][3] + m1[3][1] * m2[1][3] + m1[3][2] * m2[2][3] + m1[3][3] * m2[3][3];

		return res;
	}*/

}