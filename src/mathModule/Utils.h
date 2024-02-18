#pragma once

namespace SFE::Math {
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix;

	template<typename T>
	inline T pi() {
		return static_cast<T>(3.14159265358979323846264338327950288);
	}

	template<typename T>
	inline T twoPi() {
		return T(6.28318530717958647692528676655900576);
	}

	template<typename T>
	inline T radians(T degrees) {
		return degrees * static_cast<T>(0.01745329251994329576923690768489);//pi/180
	}

	template<typename T>
	inline T degrees(T radians) {
		return radians * static_cast<T>(57.295779513082320876798154814105);//180/pi
	}

	template<typename T>
	inline Math::Vector<T, 3> radians(const Math::Vector<T, 3>& degrees) {
		return degrees * static_cast<T>(0.01745329251994329576923690768489);//pi/180
	}
	template<typename T>
	inline Math::Vector<T,3> degrees(const Math::Vector<T, 3>& radians) {
		return radians * static_cast<T>(57.295779513082320876798154814105);//180/pi
	}

	// -------------------------

	template<typename T, typename T1>
	inline Matrix<T, 4, 4> translate(Matrix<T, 4, 4> m, const Vector<T1, 3>& v) {
		return m[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3], m;
	}

	template<typename T, typename FT>
	inline T mix(const T& x, const T& y, FT t) {
		return x * (1 - t) + y * t;
	}

	template<typename T, typename T1>
	constexpr inline Matrix<T, 4, 4> scale(Matrix<T, 4, 4> m, Vector<T1, 3> const& v) {
		return m[0] *= v[0], m[1] *= v[1], m[2] *= v[2], m;
	}

	template<typename T>
	Matrix<T, 4, 4> rotate(Matrix<T, 4, 4> const& m, T angle, Vector<T, 3> const& v)
	{
		T const a = angle;
		T const c = cos(a);
		T const s = sin(a);

		Vector<T, 3> axis(normalize(v));
		Vector<T, 3> temp(axis * (T(1) - c));

		Matrix<T, 4, 4> rotate;
		rotate[0][0] = c + temp[0] * axis[0];
		rotate[0][1] = temp[0] * axis[1] + s * axis[2];
		rotate[0][2] = temp[0] * axis[2] - s * axis[1];

		rotate[1][0] = temp[1] * axis[0] - s * axis[2];
		rotate[1][1] = c + temp[1] * axis[1];
		rotate[1][2] = temp[1] * axis[2] + s * axis[0];

		rotate[2][0] = temp[2] * axis[0] + s * axis[1];
		rotate[2][1] = temp[2] * axis[1] - s * axis[0];
		rotate[2][2] = c + temp[2] * axis[2];

		Matrix<T, 4, 4> result;
		result[0] = m[0] * rotate[0][0] + m[1] * rotate[0][1] + m[2] * rotate[0][2];
		result[1] = m[0] * rotate[1][0] + m[1] * rotate[1][1] + m[2] * rotate[1][2];
		result[2] = m[0] * rotate[2][0] + m[1] * rotate[2][1] + m[2] * rotate[2][2];
		result[3] = m[3];
		return result;
	}


	// ---------------------------

	template<typename T>
	Matrix<T, 4, 4> perspectiveRH_NO(T fovy, T aspect, T zNear, T zFar) {
		assert(abs(aspect - std::numeric_limits<T>::epsilon()) > static_cast<T>(0));

		T const tanHalfFovy = tan(fovy / static_cast<T>(2));

		Matrix<T, 4, 4 > result{static_cast<T>(0)};
		result[0][0] = static_cast<T>(1) / (aspect * tanHalfFovy);
		result[1][1] = static_cast<T>(1) / (tanHalfFovy);
		result[2][2] = -(zFar + zNear) / (zFar - zNear);
		result[2][3] = -static_cast<T>(1);
		result[3][2] = -(static_cast<T>(2) * zFar * zNear) / (zFar - zNear);
		return result;
	}

	template<typename T>
	Matrix<T, 4, 4> orthoRH_NO(T left, T right, T bottom, T top, T zNear, T zFar) {
		Matrix<T, 4, 4> result{1};
		result[0][0] = static_cast<T>(2) / (right - left);
		result[1][1] = static_cast<T>(2) / (top - bottom);
		result[2][2] = -static_cast<T>(2) / (zFar - zNear);
		result[3][0] = -(right + left) / (right - left);
		result[3][1] = -(top + bottom) / (top - bottom);
		result[3][2] = -(zFar + zNear) / (zFar - zNear);
		return result;
	}

	// ------------------------

	void globalToScreenCoords();
	
}
