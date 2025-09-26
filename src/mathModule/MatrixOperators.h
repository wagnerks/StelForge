#pragma once

#include "MatrixOperations.h"

namespace SFE::Math {
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix;

	template<typename T, size_t Size>
	struct Vector;


	template <typename T, size_t Rows, size_t Columns>
	inline bool operator==(const Matrix<T, Rows, Columns>& a, const Matrix<T, Rows, Columns>& b) {
		for (size_t i = 0; i < Columns; ++i) {
			if (a[i] != b[i]) {
				return false;
			}
		}

		return true;
	}

	template <typename T, size_t Rows, size_t Columns>
	inline bool operator!=(const Matrix<T, Rows, Columns>& a, const Matrix<T, Rows, Columns>& b) {
		return !operator==(a, b);
	}


	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> operator*(const Matrix<T, Rows, Columns>& a, OtherT b) {
		return multiply(a, b);
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> operator/(const Matrix<T, Rows, Columns>& a, OtherT b) {
		return divide(a, b);
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> operator-(const Matrix<T, Rows, Columns>& a, OtherT b) {
		return subtract(a, b);
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> operator+(const Matrix<T, Rows, Columns>& a, OtherT b) {
		return add(a, b);
	}


	template <typename T, size_t Rows, size_t Columns, size_t OtherCols>
	inline Matrix<T, Columns, OtherCols> operator*(const Matrix<T, Rows, Columns>& a, const Matrix<T, Columns, OtherCols>& other) {
		Matrix<T, Columns, OtherCols> result = {};

		for (size_t i = 0; i < OtherCols; ++i) {
			for (size_t j = 0; j < Columns; ++j) {
				for (size_t k = 0; k < Columns; ++k) {
					result[i][j] += a[k][j] * other[i][k];
				}
			}
		}

		return result;
	}

	template <typename T, size_t Rows, size_t Columns, size_t OtherCols>
	inline Matrix<T, Columns, OtherCols> operator/(const Matrix<T, Rows, Columns>& a, const Matrix<T, Columns, OtherCols>& other) {
		Matrix<T, Columns, OtherCols> result = {};

		for (size_t i = 0; i < OtherCols; ++i) {
			for (size_t j = 0; j < Columns; ++j) {
				for (size_t k = 0; k < Columns; ++k) {
					result[i][j] += a[k][j] / other[i][k];
				}
			}
		}

		return result;
	}

	template <typename T>
	inline constexpr Matrix<T, 4, 4> operator*(const Matrix<T, 4, 4>& a, const Matrix<T, 4, 4>& other)  {
		return
		{
			a[0] * other[0][0] + a[1] * other[0][1] + a[2] * other[0][2] + a[3] * other[0][3],
			a[0] * other[1][0] + a[1] * other[1][1] + a[2] * other[1][2] + a[3] * other[1][3],
			a[0] * other[2][0] + a[1] * other[2][1] + a[2] * other[2][2] + a[3] * other[2][3],
			a[0] * other[3][0] + a[1] * other[3][1] + a[2] * other[3][2] + a[3] * other[3][3]
		};
	}

	template <typename T>
	inline constexpr Matrix<T, 3, 3> operator*(const Matrix<T, 3, 3>& a, const Matrix<T, 3, 3>& other) {
		return
		{
			a[0] * other[0][0] + a[1] * other[0][1] + a[2] * other[0][2],
			a[0] * other[1][0] + a[1] * other[1][1] + a[2] * other[1][2],
			a[0] * other[2][0] + a[1] * other[2][1] + a[2] * other[2][2],
		};
	}

	template <typename T, size_t Rows, size_t Columns, size_t OtherCols>
	inline Matrix<T, Columns, OtherCols> operator*=(Matrix<T, Rows, Columns>& a, const Matrix<T, Columns, OtherCols>& other) {
		return a = a * other, a;
	}

	template <typename T, size_t Rows, size_t Columns, typename OtherT>
	inline Vector<T, Columns> operator*(const Matrix<T, Rows, Columns>& a, const Vector<OtherT, Columns>& other) {
		Vector<T, Columns> result = {};

		for (size_t j = 0; j < Columns; ++j) {
			for (size_t k = 0; k < Columns; ++k) {
				result[j] += a[k][j] * other[k];
			}
		}

		return result;
	}

	template <typename T, typename OtherT>
	inline constexpr Vector<T, 2> operator*(const Matrix<T, 2, 2>& a, const Vector<OtherT, 2>& other) {
		return {
			a[0][0] * other[0] + a[1][0] * other[1],
			a[0][1] * other[0] + a[1][1] * other[1],
		};
	}

	template <typename T, typename OtherT>
	inline constexpr Vector<T, 3> operator*(const Matrix<T, 3, 3>& a, const Vector<OtherT, 3>& other) {
		return {
			a[0][0] * other[0] + a[1][0] * other[1] + a[2][0] * other[2],
			a[0][1] * other[0] + a[1][1] * other[1] + a[2][1] * other[2],
			a[0][2] * other[0] + a[1][2] * other[1] + a[2][2] * other[2]
		};
	}

	//helper to multiply transform with 2D point
	template <typename T, typename OtherT>
	inline constexpr Vector<T, 2> operator*(const Matrix<T, 3, 3>& a, const Vector<OtherT, 2>& other) {
		return {
			a[0][0] * other[0] + a[1][0] * other[1] + a[2][0],
			a[0][1] * other[0] + a[1][1] * other[1] + a[2][1],
		};
	}

	template <typename T, typename OtherT>
	inline constexpr Vector<T, 4> operator*(const Matrix<T, 4, 4>& a, const Vector<OtherT, 4>& other) {
		return {
			a[0][0] * other[0] + a[1][0] * other[1] + a[2][0] * other[2] + a[3][0] * other[3],
			a[0][1] * other[0] + a[1][1] * other[1] + a[2][1] * other[2] + a[3][1] * other[3],
			a[0][2] * other[0] + a[1][2] * other[1] + a[2][2] * other[2] + a[3][2] * other[3],
			a[0][3] * other[0] + a[1][3] * other[1] + a[2][3] * other[2] + a[3][3] * other[3],
		};
	}

	//helper to multiply transform with 3D point
	template <typename T, typename OtherT>
	inline constexpr Vector<T, 3> operator*(const Matrix<T, 4, 4>& a, const Vector<OtherT, 3>& other) {
		return {
			a[0][0] * other[0] + a[1][0] * other[1] + a[2][0] * other[2] + a[3][0],
			a[0][1] * other[0] + a[1][1] * other[1] + a[2][1] * other[2] + a[3][1],
			a[0][2] * other[0] + a[1][2] * other[1] + a[2][2] * other[2] + a[3][2],
		};
	}
}
