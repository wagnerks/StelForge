#pragma once

#include <complex>
#include <assert.h>

namespace SFE::Math {
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix;

	template <typename T, size_t Size>
	struct Vector;

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> multiply(Matrix<T, Rows, Columns> a, OtherT b) {
		for (size_t i = 0; i < Columns; i++) {
			a[i] *= b;
		}
		return a;
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> divide(Matrix<T, Rows, Columns> a, OtherT b) {
		for (size_t i = 0; i < Columns; i++) {
			a[i] /= b;
		}
		return a;
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> subtract(Matrix<T, Rows, Columns> a, OtherT b) {
		for (size_t i = 0; i < Columns; i++) {
			a[i] -= b;
		}
		return a;
	}

	template <typename T, typename OtherT, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> add(Matrix<T, Rows, Columns> a, OtherT b) {
		for (size_t i = 0; i < Columns; i++) {
			a[i] += b;
		}
		return a;
	}

	// /////

	template <typename T, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> negate(Matrix<T, Rows, Columns> a) {
		for (size_t i = 0; i < Columns; ++i) {
			a[i] = -a[i];
		}
		return a;
	}

	template <typename T, size_t Rows, size_t Columns>
	inline Matrix<T, Rows, Columns> transpose(const Matrix<T, Rows, Columns>& matrix) {
		Matrix<T, Rows, Columns> result {};
		for (size_t i = 0; i < Columns; ++i) {
			for (size_t j = 0; j < Rows; ++j) {
				result[i][j] = matrix[j][i];
			}
		}

		return result;
	}

	template <typename T>
	Matrix<T, 3, 3> inverse(Matrix<T, 3, 3> m) {
		T determinant = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
			m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
			m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

		if (determinant == 0) {
			assert(false);
			// Matrix is not invertible
			return {};
		}

		T invDeterminant = 1.0 / determinant;

		// Calculate elements of the inverted matrix
		Matrix<T, 3, 3> temp;

		temp[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDeterminant;
		temp[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDeterminant;
		temp[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDeterminant;
		temp[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDeterminant;
		temp[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDeterminant;
		temp[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDeterminant;
		temp[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDeterminant;
		temp[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDeterminant;
		temp[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDeterminant;

		return temp;
	}

	template <typename T>
	Matrix<T, 4, 4> inverse(Matrix<T, 4, 4> m) {
		T coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
		T coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
		T coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];

		T coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
		T coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
		T coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];

		T coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
		T coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
		T coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];

		T coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
		T coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
		T coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];

		T coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
		T coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
		T coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];

		T coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
		T coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
		T coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

		Vector<T, 4> fac0(coef00, coef00, coef02, coef03);
		Vector<T, 4> fac1(coef04, coef04, coef06, coef07);
		Vector<T, 4> fac2(coef08, coef08, coef10, coef11);
		Vector<T, 4> fac3(coef12, coef12, coef14, coef15);
		Vector<T, 4> fac4(coef16, coef16, coef18, coef19);
		Vector<T, 4> fac5(coef20, coef20, coef22, coef23);

		Vector<T, 4> vec0(m[1][0], m[0][0], m[0][0], m[0][0]);
		Vector<T, 4> vec1(m[1][1], m[0][1], m[0][1], m[0][1]);
		Vector<T, 4> vec2(m[1][2], m[0][2], m[0][2], m[0][2]);
		Vector<T, 4> vec3(m[1][3], m[0][3], m[0][3], m[0][3]);

		Vector<T, 4> inv0(vec1 * fac0 - vec2 * fac1 + vec3 * fac2);
		Vector<T, 4> inv1(vec0 * fac0 - vec2 * fac3 + vec3 * fac4);
		Vector<T, 4> inv2(vec0 * fac1 - vec1 * fac3 + vec3 * fac5);
		Vector<T, 4> inv3(vec0 * fac2 - vec1 * fac4 + vec2 * fac5);

		Vector<T, 4> signA(+1, -1, +1, -1);
		Vector<T, 4> signB(-1, +1, -1, +1);
		Matrix<T, 4, 4> inverse(inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB);

		Vector<T, 4> row0(inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0]);

		Vector<T, 4> dot0(m[0] * row0);
		T dot1 = (dot0.x + dot0.y) + (dot0.z + dot0.w);

		T oneOverDeterminant = static_cast<T>(1) / dot1;

		return inverse * oneOverDeterminant;
	}
}
