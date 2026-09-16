#pragma once

#include <cmath>
#include <limits>

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

	// Rodrigues' formula; axis must be unit length
	template<typename T>
	inline Vector<T, 3> rotate(const Vector<T, 3>& v, T angle, const Vector<T, 3>& axis) {
		const T c = std::cos(angle);
		const T s = std::sin(angle);
		return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (static_cast<T>(1) - c));
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
	inline Vector<T,3> pointsToVector(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return p2 - p1;
	}

	template<typename T>
	inline T distanceSqr(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return lengthSquared(p2 - p1);
	}

	template<typename T>
	inline T distance(const Vector<T, 3>& p1, const Vector<T, 3>& p2) {
		return std::sqrt(distanceSqr(p1, p2));
	}
}