#pragma once
#include "VectorOperations.h"

namespace SFE::Math {
	template<class T, size_t Size>
	struct Vector;

	template <typename T, size_t Rows>
	inline Vector<T, Rows> operator-(Vector<T, Rows> a) {
		return negate(a);
	}

	// math operators
	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator+=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = add(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator-=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = subtract(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator*=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = multiply(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator/=(Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return a = divide(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator*(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator/(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return divide(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator-(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator+(const Vector<T, Rows>& a, const Vector<Other, Rows>& b) {
		return add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator+=(Vector<T, Rows>& a, Other b) {
		return a = add(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator-=(Vector<T, Rows>& a, Other b) {
		return a = subtract(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator*=(Vector<T, Rows>& a, Other b) {
		return a = multiply(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator/=(Vector<T, Rows>& a, Other b) {
		return a = divide(a, b), a;
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator+(const Vector<T, Rows>& a, Other b) {
		return add(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator-(const Vector<T, Rows>& a, Other b) {
		return subtract(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator*(const Vector<T, Rows>& a, Other b) {
		return multiply(a, b);
	}

	template <typename T, size_t Rows, typename Other>
	inline Vector<T, Rows> operator/(const Vector<T, Rows>& a, Other b) {
		return divide(a, b);
	}

	//relational operators

	template <typename T, size_t Size>
	inline bool operator==(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		for (size_t i = 0; i < Size; ++i) {
			if (std::abs(a[i] - b[i]) > std::numeric_limits<T>::epsilon()) {
				return false;
			}
		}

		return true;
	}

	template <typename T, size_t Size>
	inline bool operator!=(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		return !operator==(a, b);
	}

	template <typename T, size_t Size>
	inline bool operator<(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		for (size_t i = 0; i < Size; ++i) {
			if (a[i] < b[i]) {
				return true;
			}
			if (a[i] == b[i] && i != Size - 1) {
				continue;
			}
			return false;
		}

		return false;
	}
	template <typename T, size_t Size>
	inline bool operator>(const Vector<T, Size>& a, const Vector<T, Size>& b) {
		for (size_t i = 0; i < Size; ++i) {
			if (a[i] > b[i]) {
				continue;
			}
			return false;
		}

		return true;
	}
	
}