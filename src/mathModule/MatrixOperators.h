#pragma once

#include "MatrixOperations.h"

namespace Engine::Math {
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
}
