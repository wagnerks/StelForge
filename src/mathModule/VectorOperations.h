#pragma once

#include <complex>

namespace SFE::Math {
	template<class T, size_t Size>
	struct Vector;

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> multiply(Vector<T, Rows> a, const Vector<Other, Rows>& b) {
		for (size_t i = 0; i < Rows; ++i) {
			a[i] *= b[i];
		}

		return a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> divide(Vector<T, Rows> a, const Vector<Other, Rows>& b) {
		for (size_t i = 0; i < Rows; ++i) {
			a[i] /= b[i];
		}

		return a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> subtract(Vector<T, Rows> a, const Vector<Other, Rows>& b) {
		for (size_t i = 0; i < Rows; ++i) {
			a[i] -= b[i];
		}

		return a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> add(Vector<T, Rows> a, const Vector<Other, Rows>& b) {
		for (size_t i = 0; i < Rows; ++i) {
			a[i] += b[i];
		}

		return a;
	}

	///////

	template <typename T, typename OtherT, size_t Size>
	inline Vector<T, Size> multiply(Vector<T, Size> a, OtherT b) {
		for (size_t i = 0; i < Size; i++) {
			a[i] *= b;
		}

		return a;
	}

	template <typename T, typename OtherT, size_t Size>
	inline Vector<T, Size> divide(Vector<T, Size> a, OtherT b) {
		for (size_t i = 0; i < Size; i++) {
			a[i] /= b;
		}

		return a;
	}

	template <typename T, typename OtherT, size_t Size>
	inline Vector<T, Size> subtract(Vector<T, Size> a, OtherT b) {
		for (size_t i = 0; i < Size; i++) {
			a[i] -= b;
		}

		return a;
	}

	template <typename T, typename OtherT, size_t Size>
	inline Vector<T, Size> add(Vector<T, Size> a, OtherT b) {
		for (size_t i = 0; i < Size; i++) {
			a[i] += b;
		}

		return a;
	}


	// /////

	template <typename T, size_t Rows>
	inline T lengthSquared(const Vector<T, Rows>& a) {
		T result = {};
		for (size_t i = 0; i < Rows; ++i) {
			result += a[i] * a[i];
		}
		return result;
	}

	template <typename T, size_t Rows>
	inline T length(const Vector<T, Rows>& a) {
		return std::sqrt(lengthSquared(a));
	}

	template <typename T, size_t Rows>
	constexpr inline T dot(const Vector<T, Rows>& a, const Vector<T, Rows>& b) {
		T result = {};
		for (size_t i = 0; i < Rows; ++i) {
			result += a[i] * b[i];
		}
		return result;
	}

	template<typename T>
	constexpr inline Vector<T, 3> cross(const Vector<T, 3>& A, const Vector<T, 3>& B) {
		Vector<T, 3> C;
		C.x = A.y * B.z - A.z * B.y;
		C.y = A.z * B.x - A.x * B.z;
		C.z = A.x * B.y - A.y * B.x;

		return C;
	}

	template<typename T>
	Vector<T, 3> rotate(const Vector<T, 3>& v, float angle, const Vector<T, 3>& axis) {
		float cos_angle = std::cos(angle);
		float sin_angle = std::sin(angle);
		Vector<T, 3> cross_term = cross(axis, v);
		return {
			v.x * cos_angle + cross_term.x * sin_angle + axis.x * dot(axis, v) * (1 - cos_angle),
			v.y * cos_angle + cross_term.y * sin_angle + axis.y * dot(axis, v) * (1 - cos_angle),
			v.z * cos_angle + cross_term.z * sin_angle + axis.z * dot(axis, v) * (1 - cos_angle)
		};
	}

	template<typename T>
	inline void projectVectorsToCommonPlane(Vector<T, 3>& A, Vector<T, 3>& B) {
		/*auto A_normalized = normalize(A);
		auto B_normalized = normalize(B);
		
		auto axis = cross(A_normalized, B_normalized);
		float angle = std::acos(dot(A_normalized, B_normalized));
		
		A = rotate(A, angle, axis);
		B = rotate(B, angle, axis);*/


		// Normalize vectors A and B
		auto A_normalized = normalize(A);
		auto B_normalized = normalize(B);

		// Calculate the rotation axis (perpendicular to A and B)
		auto axis = cross(A_normalized, B_normalized);

		// Calculate the angle between vectors A and B using the arc tangent function
		float angle = std::atan2(length(cross(A_normalized, B_normalized)), dot(A_normalized, B_normalized));

		// Rotate vectors A and B to project them onto the common plane
		A = rotate(A, angle, axis);
		B = rotate(B, angle, axis);
	}

	template <typename T, size_t Rows>
	inline Vector<T, Rows> normalize(const Vector<T, Rows>& a) {
		auto l = length(a);
		if (l != 0) {
			return divide(a, l);
		}
		return {};
	}

	template <typename T, size_t Size>
	inline Vector<T, Size> negate(Vector<T, Size> a) {
		for (size_t i = 0; i < Size; ++i) {
			a[i] = -a[i];
		}

		return a;
	}

	template<typename T>
	inline T fastSqrt(T val) {
		return static_cast<T>(1) / sqrt(val);
	}

	template<typename T>
	inline Vector<T,3> pointsToVector(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return p2 - p1;
	}

	template<typename T>
	inline T distanceSqr(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return lengthSquared(p2 - p1);
	}

	template<typename T>
	inline T distance(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return sqrt(distanceSqr(p1, p2));
	}
}