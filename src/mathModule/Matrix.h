#pragma once

#include <detail/type_mat2x2.hpp>

#include "MatrixOperators.h"

#define MATRIX_DEFAULT_METHODS() \
	constexpr inline auto& operator[](size_t column) {	return m[column];}\
	constexpr inline const auto& operator[](size_t column) const { return m[column]; }\
	constexpr inline const Matrix& transpose() { return *this = Math::transpose(*this), *this; }\
	constexpr inline const T* data() const { return m[0].data(); }

#define GLMVEC 0

namespace SFE::Math {
#if GLMVEC
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix : public glm::mat<Columns, Rows,T>{};

	template <typename T>
	struct Matrix<T,4,4> : public glm::mat<4, 4, T> {
		Matrix(T a) : glm::mat<4,4,T>(a){}
		Matrix() : glm::mat<4, 4, T>() {}
		Matrix(Matrix<T, 3, 3> a) : glm::mat<4,4,T>(a){}
		constexpr Matrix(const Vector<T, 4>& a, const Vector<T, 4>& b, const Vector<T, 4>& c, const Vector<T, 4>& d) : glm::mat<4,4,T>(a,b,c,d){}
		inline const Matrix& transpose() { return *this = Math::transpose(*this), *this; }
		inline const T* data() const { return &glm::mat<4, 4, T>::operator[](0).x; }
	};

	template <typename T>
	struct Matrix<T, 3, 3> : public glm::mat<3, 3, T> {
		Matrix(T a) : glm::mat<3, 3, T>(a) {}
		Matrix(Matrix<T, 4, 4> a) : glm::mat<3, 3, T>(a) {}
		Matrix() : glm::mat<3, 3, T>() {}
		inline const Matrix& transpose() { return *this = Math::transpose(*this), *this; }
		inline const T* data() const { return &glm::mat<3, 3, T>::operator[](0).x; }
	};
#else
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix {
		Vector<T,Rows> m[Columns];

		MATRIX_DEFAULT_METHODS();
	};

	template <typename T>
	struct Matrix<T, 3, 3> {
		Vector<T, 3> m[3];

		MATRIX_DEFAULT_METHODS();

		constexpr Matrix() {}
		constexpr Matrix(T s) : m{ Vector<T, 3> {s,static_cast<T>(0),static_cast<T>(0)}, Vector<T, 3> {static_cast<T>(0), s, static_cast<T>(0)}, Vector<T, 3> {static_cast<T>(0), static_cast<T>(0), s} } {}
		constexpr Matrix(const Vector<T, 3>& a, const Vector<T, 3>& b, const Vector<T, 3>& c) : m{ a,b,c } {}
		constexpr Matrix(const Matrix<T, 4, 4>& mat) : m{ Vector<T, 3>(mat[0]), Vector<T, 3>(mat[1]), Vector<T, 3>(mat[2])} {}
	};

	template <typename T>
	struct Matrix<T, 4, 4> {
		Vector<T, 4> m[4];

		MATRIX_DEFAULT_METHODS();

		constexpr Matrix(){}
		constexpr Matrix(T s) : m { Vector<T, 4> {s,0,0,0}, Vector<T, 4> {0, s, 0, 0}, Vector<T, 4> {0, 0, s, 0}, Vector<T, 4> {0, 0, 0, s}}{}
		constexpr Matrix(const Vector<T, 4>& a, const Vector<T, 4>& b, const Vector<T, 4>& c, const Vector<T, 4>& d) : m{ a,b,c,d }{}

		template<typename T1>
		constexpr Matrix(const Matrix<T1,3,3>& mat) : m{ Vector<T, 4>(mat[0],T{}), Vector<T, 4>(mat[1],T{}), Vector<T, 4>(mat[2],T{}), Vector<T, 4>{}} {}
	};
#endif
}
