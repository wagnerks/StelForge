#pragma once

#include "VectorOperators.h"

#define VECTOR_DEFAULT_METHODS()														\
	inline const auto& operator[](size_t index) const {	return m[index]; }				\
	inline auto& operator[](size_t index) {	return m[index]; }							\
	inline T length() const {	return Math::length(*this);	}							\
	inline T lengthSquared() const { return Math::lengthSquared(*this);	}				\
	inline const Vector& normalize() { return *this = Math::normalize(*this), *this; }	\
	inline const T* data() const { return &m[0];}

#define GLMVEC 0

namespace SFE::Math {
#if GLMVEC
	template<class T, size_t Size>
	struct Vector : public glm::vec<Size, T>{};

	template <typename T>
	struct Vector<T, 2> : public glm::vec<2,T> {
		constexpr Vector() : glm::vec<2, T>() {}
		constexpr Vector(T x, T y) : glm::vec<2, T, glm::qualifier::packed_highp>(x, y) {}
		inline T length() const { return Math::length(*this); }							
		inline T lengthSquared() const { return Math::lengthSquared(*this); }				
		inline const Vector& normalize() { return *this = Math::normalize(*this), *this; }
		inline const T* data() const { return &(glm::vec<2, T>::x); }
	};

	template <typename T>
	struct Vector<T, 3> : public glm::vec<3,T> {
		constexpr Vector() : glm::vec<3, T>() {}
		constexpr Vector(T x, T y, T z) : glm::vec<3, T, glm::qualifier::packed_highp>(x, y, z) {}
		constexpr Vector(T x) : glm::vec<3, T, glm::qualifier::packed_highp>(x) {}

		constexpr Vector(Vector<T, 4> a) : glm::vec<3, T, glm::qualifier::packed_highp>(a) {}
		constexpr Vector(glm::vec<4, T> a) : glm::vec<3, T, glm::qualifier::packed_highp>(a) {}

		inline T length() const { return Math::length(*this); }
		inline T lengthSquared() const { return Math::lengthSquared(*this); }
		inline const Vector& normalize() { return *this = Math::normalize(*this), *this; }
		inline const T* data() const { return &glm::vec<3, T>::operator[](0);}
	};
	template <typename T>
	struct Vector<T, 4> : public glm::vec<4,T> {
		constexpr Vector() : glm::vec<4,T>(){}
		constexpr Vector(T x, T y, T z, T w) : glm::vec<4, T, glm::qualifier::packed_highp>(x, y, z, w) {}
		constexpr Vector(T x) : glm::vec<4, T, glm::qualifier::packed_highp>(x) {}

		constexpr Vector(Vector<T, 3> a, T b) : glm::vec<4, T, glm::qualifier::packed_highp>(a, b) {}
		constexpr Vector(glm::vec<4, T> a) : glm::vec<4, T, glm::qualifier::packed_highp>(a) {}
		inline T length() const { return Math::length(*this); }
		inline T lengthSquared() const { return Math::lengthSquared(*this); }
		inline const Vector& normalize() { return *this = Math::normalize(*this), *this; }
		inline const T* data() const { return &glm::vec<4, T>::operator[](0); }
	};
#else
	template<class T, size_t Size>
	struct Vector {
		T m[Size];

		VECTOR_DEFAULT_METHODS();
	};

	template <typename T>
	struct Vector<T, 2> {
		union {
			struct {
				T x, y;
			};
			T m[2];
		};

		VECTOR_DEFAULT_METHODS();
	};

	template <typename T>
	struct Vector<T, 3> {
		union {
			struct {
				T x, y, z;
			};
			struct {
				T r, g, b;
			};
			T m[3];
		};

		VECTOR_DEFAULT_METHODS();
		
		constexpr Vector(T x, T y, T z) : x(x), y(y), z(z) {}
		constexpr Vector(T x) : x(x), y(x), z(x) {}
		constexpr Vector() : x{}, y{}, z{} {}

		template<typename OtherT>
		constexpr Vector(const Vector<OtherT, 4>& vec) : x(vec.x), y(vec.y), z(vec.z) {}

		template<typename OtherT>
		constexpr Vector(const Vector<OtherT, 3>& vec) : x(vec.x), y(vec.y), z(vec.z) {}
	};

	template <typename T>
	struct Vector<T, 4> {
		union {
			struct {
				T r, g, b, a;
			};
			struct {
				T x, y, z, w;
			};
			Vector<T, 3> xyz;
			T m[4];
		};

		VECTOR_DEFAULT_METHODS();

		template<typename T1, typename T2>
		constexpr Vector(const Vector<T1, 3>& vec3, T2 w) : x{ vec3.x }, y{ vec3.y}, z(vec3.z), w(w) {}

		template<typename T1>
		constexpr Vector(const Vector<T1, 4>& vec) : x(vec.x), y(vec.y), z(vec.z), w(vec.w) {}

		constexpr Vector() : x{}, y{}, z{}, w{} {}
		constexpr Vector(T x, T y, T z, T w) : x{ x }, y{ y }, z{ z }, w{ w } {}
	};
#endif
}