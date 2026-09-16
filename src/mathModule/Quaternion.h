#pragma once

#include <algorithm>

#include "Utils.h"
#include "VectorOperations.h"

namespace SFE::Math {
	template <typename T, size_t Rows, size_t Columns>
	struct Matrix;

	template <typename T, size_t Size>
	struct Vector;

	template<typename T>
	struct Quaternion {
		T w{1}, x{}, y{}, z{};

		Matrix<T, 3, 3> toRotateMatrix3() const {
			Matrix<T, 3, 3> result(T{ 1 });
			T qxx(x * x);
			T qyy(y * y);
			T qzz(z * z);
			T qxz(x * z);
			T qxy(x * y);
			T qyz(y * z);
			T qwx(w * x);
			T qwy(w * y);
			T qwz(w * z);

			result[0][0] = T{1} - T{2} * (qyy + qzz);
			result[0][1] = T{2} * (qxy + qwz);
			result[0][2] = T{2} * (qxz - qwy);

			result[1][0] = T{2} * (qxy - qwz);
			result[1][1] = T{1} - T{2} * (qxx + qzz);
			result[1][2] = T{2} * (qyz + qwx);

			result[2][0] = T{2} * (qxz + qwy);
			result[2][1] = T{2} * (qyz - qwx);
			result[2][2] = T{1} - T{2} * (qxx + qyy);

			return result;
		}

		Matrix<T, 4, 4> toMat4() const {
			Matrix<T, 4, 4> result(toRotateMatrix3());
			result[3][3] = T{1};
			return result;
		}

		Vector<T,3> getAxis() const {
			auto normalized = *this;
			normalized.normalize();
			const T sinAngle = std::sqrt(std::max(T{0}, T{1} - normalized.w * normalized.w));
			const Vector<T, 3> xyz{ normalized.x, normalized.y, normalized.z };
			return sinAngle < static_cast<T>(0.001) ? xyz : xyz / sinAngle;
		}

		void eulerToQuaternion(const Vector<T,3>& eulerDegrees) {
			eulerToQuaternion(Math::radians(eulerDegrees.x), Math::radians(eulerDegrees.y), Math::radians(eulerDegrees.z));
		}

		void eulerToQuaternion(T roll, T pitch, T yaw) {
			const T half = static_cast<T>(0.5);
			T cy = std::cos(yaw * half);
			T sy = std::sin(yaw * half);
			T cp = std::cos(pitch * half);
			T sp = std::sin(pitch * half);
			T cr = std::cos(roll * half);
			T sr = std::sin(roll * half);

			w = cy * cp * cr + sy * sp * sr;
			x = cy * cp * sr - sy * sp * cr;
			y = sy * cp * sr + cy * sp * cr;
			z = sy * cp * cr - cy * sp * sr;
		}

		inline Vector<T,3> toEuler() const {
			Vector<T, 3> euler;

			// Roll (x-axis rotation)
			euler.x = std::atan2(2 * (w*x + y*z), w*w - x*x - y*y + z*z);

			// Pitch (y-axis rotation); clamped, rounding pushes it past 1 at gimbal lock
			euler.y = std::asin(std::clamp(2 * (w * y - z * x), T{-1}, T{1}));

			// Yaw (z-axis rotation)
			euler.z = std::atan2(2 * (w * z + x * y), w * w + x * x - y * y - z * z);
			return euler;
		}

		inline T magnitude() const {
			return std::sqrt(x * x + y * y + z * z + w * w);
		}

		void normalize() {
			T mag = magnitude();
			if (mag > 0) {
				x /= mag;
				y /= mag;
				z /= mag;
				w /= mag;
			}
		}

		void changeAxis(const Math::Vector<T,3>& newAxis) {
			x = newAxis.x;
			y = newAxis.y;
			z = newAxis.z;

			normalize();
		}

		void matrixToQuaternion(const Matrix<T, 3, 3>& matrix) {
			matrixToQuaternionImpl(matrix);
		}

		// both expect a unit quaternion
		inline Vector<T, 3> globalToLocal(const Vector<T, 3>& v) const {
			return inverseQuaternion().rotateVector(v);
		}

		// q * v * q^-1 expanded: v + 2w(u x v) + 2u x (u x v)
		inline Vector<T, 3> rotateVector(const Vector<T, 3>& v) const {
			const Vector<T, 3> u{ x, y, z };
			const Vector<T, 3> t = cross(u, v) * T{2};
			return v + t * w + cross(u, t);
		}

		inline Quaternion inverseQuaternion() const {
			return { w, -x, -y, -z };
		}

		inline Quaternion operator*(const Quaternion& q2) const{
			return {
				w * q2.w - x * q2.x - y * q2.y - z * q2.z, //w
				w * q2.x + x * q2.w + y * q2.z - z * q2.y, //x
				w * q2.y - x * q2.z + y * q2.w + z * q2.x, //y
				w * q2.z + x * q2.y - y * q2.x + z * q2.w  //z
			};
		}

		Quaternion& operator+=(const Vector<T, 3>& vec) {
			Quaternion q(0, vec.x, vec.y, vec.z);
			q = q * *this;
			const T half = static_cast<T>(0.5);
			x += q.x * half;
			y += q.y * half;
			z += q.z * half;
			w += q.w * half;

			normalize();

			return *this;
		}

		constexpr Quaternion(){}

		Quaternion(T x, T y, T z) {
			eulerToQuaternion(x, y, z);
		}

		constexpr Quaternion(T w, T x, T y, T z) : w{ w }, x{ x }, y{ y }, z{ z } {}

		constexpr Quaternion(T w, const Vector<T,3>& vec ) : w{ w }, x{ vec.x }, y{ vec.y }, z{ vec.z } {}

		// shortest rotation taking unit vector axis onto unit vector direction
		// (1 + cos, sin * axis) normalized is the half-angle quaternion, no acos to lose precision near a half turn
		Quaternion(const Vector<T, 3>& direction, const Vector<T, 3>& axis) {
			w = T{1} + dot(axis, direction);
			auto rotationAxis = cross(axis, direction);
			if (w < static_cast<T>(1e-6)) {
				// opposite vectors: turn half a circle around any axis perpendicular to them
				w = T{0};
				rotationAxis = cross(axis, std::abs(axis.x) < static_cast<T>(0.9) ? Vector<T, 3>{ 1, 0, 0 } : Vector<T, 3>{ 0, 1, 0 });
			}
			x = rotationAxis.x;
			y = rotationAxis.y;
			z = rotationAxis.z;
			normalize();
		}

	private:
		void matrixToQuaternionImpl(const Matrix<T,3,3>& m) {
			// m[column][row]
			const T one = T{1};
			const T quarter = static_cast<T>(0.25);
			T trace = m[0][0] + m[1][1] + m[2][2];

			if (trace > 0) {
				T s = static_cast<T>(0.5) / std::sqrt(trace + one);
				w = quarter / s;
				x = (m[1][2] - m[2][1]) * s;
				y = (m[2][0] - m[0][2]) * s;
				z = (m[0][1] - m[1][0]) * s;
			}
			else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
				T s = T{2} * std::sqrt(one + m[0][0] - m[1][1] - m[2][2]);
				w = (m[1][2] - m[2][1]) / s;
				x = quarter * s;
				y = (m[0][1] + m[1][0]) / s;
				z = (m[0][2] + m[2][0]) / s;
			}
			else if (m[1][1] > m[2][2]) {
				T s = T{2} * std::sqrt(one + m[1][1] - m[0][0] - m[2][2]);
				w = (m[2][0] - m[0][2]) / s;
				x = (m[0][1] + m[1][0]) / s;
				y = quarter * s;
				z = (m[1][2] + m[2][1]) / s;
			}
			else {
				T s = T{2} * std::sqrt(one + m[2][2] - m[0][0] - m[1][1]);
				w = (m[0][1] - m[1][0]) / s;
				x = (m[0][2] + m[2][0]) / s;
				y = (m[1][2] + m[2][1]) / s;
				z = quarter * s;
			}

			normalize();
		}
	};

	template <typename T>
	inline Quaternion<T> normalize(Quaternion<T> quat) {
		quat.normalize();
		return quat;
	}

	template <typename T>
	inline Quaternion<T> slerp(const Quaternion<T>& q0, const Quaternion<T>& q1, T inFraction) {
		const T one = T{1};
		T sign = one;
		T cosOmega = std::clamp(q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z, -one, one);

		if (cosOmega < 0) {
			cosOmega = -cosOmega;
			sign = -one;
		}

		T scale0, scale1;
		if (one - cosOmega > static_cast<T>(0.0001)) {
			const T omega = std::acos(cosOmega);
			const T sinOmega = std::sin(omega);
			scale0 = std::sin((one - inFraction) * omega) / sinOmega;
			scale1 = sign * std::sin(inFraction * omega) / sinOmega;
		}
		else {
			scale0 = one - inFraction;
			scale1 = sign * inFraction;
		}

		return normalize(Quaternion<T>{
			scale0 * q0.w + scale1 * q1.w,
			scale0 * q0.x + scale1 * q1.x,
			scale0 * q0.y + scale1 * q1.y,
			scale0 * q0.z + scale1 * q1.z
		});
	}
}
