#pragma once
#include "VectorOperations.h"

namespace SFE::Math {
	template<class T, size_t Size>
	struct Vector;

	template <typename T, size_t Rows>
	constexpr inline Vector<T, Rows> operator-(Vector<T, Rows> a) {
		return negate(a);
	}

	// math operators
	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator+=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator-=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator*=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator/=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = divide(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator*(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator/(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return divide(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator-(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator+(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator+=(Vector<T, Rows>& a, Other b) {
		return a = add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator-=(Vector<T, Rows>& a, Other b) {
		return a = subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator*=(Vector<T, Rows>& a, Other b) {
		return a = multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows>& operator/=(Vector<T, Rows>& a, Other b) {
		return a = divide(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator+(const Vector<T, Rows>& a, Other b) {
		return add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator-(const Vector<T, Rows>& a, Other b) {
		return subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator*(const Vector<T, Rows>& a, Other b) {
		return multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	constexpr inline Vector<T, Rows> operator/(const Vector<T, Rows>& a, Other b) {
		return divide(a, b);
	}

	//relational operators

	template <typename T, size_t Size>
	constexpr inline bool operator==(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		for (size_t i = 0; i < Size; ++i) {
			if (a[i] != b[i]) {
				return false;
			}
		}

		return true;
	}

	template <typename T, size_t Size>
	constexpr inline bool operator!=(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		return !operator==(a, b);
	}

	// lexicographic, so vectors can key ordered containers
	template <typename T, size_t Size>
	constexpr inline bool operator<(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		for (size_t i = 0; i < Size; ++i) {
			if (a[i] != b[i]) {
				return a[i] < b[i];
			}
		}
		return false;
	}

	template <typename T, size_t Size>
	constexpr inline bool operator>(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		return b < a;
	}
}