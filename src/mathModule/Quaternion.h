#pragma once

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
			Matrix<T, 4, 4> result(T{ 1 });
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
			result[3][3] = 1.f;

			return result;
		}

		Vector<T,3> getAxis() const {
			auto normalized = normalize(*this);
			Vector<T, 3> axis;
			//// Compute the angle of rotation
			//angle = 2 * std::acos(normalized.w);

			float sinAngle = std::sqrt(1 - normalized.w * normalized.w);
			if (std::fabs(sinAngle) < 0.001) {
				axis.x = normalized.x;
				axis.y = normalized.y;
				axis.z = normalized.z;
			}
			else {
				axis.x = normalized.x / sinAngle;
				axis.y = normalized.y / sinAngle;
				axis.z = normalized.z / sinAngle;
			}

			return axis;
		}

		void eulerToQuaternion(const Vector<T,3>& eulerDegrees) {
			eulerToQuaternion(Math::radians(eulerDegrees.x), Math::radians(eulerDegrees.y), Math::radians(eulerDegrees.z));
		}

		void eulerToQuaternion(T roll, T pitch, T yaw) {
			T cy = static_cast<T>(cos(yaw * 0.5));
			T sy = static_cast<T>(sin(yaw * 0.5));
			T cp = static_cast<T>(cos(pitch * 0.5));
			T sp = static_cast<T>(sin(pitch * 0.5));
			T cr = static_cast<T>(cos(roll * 0.5));
			T sr = static_cast<T>(sin(roll * 0.5));

			w = cy * cp * cr + sy * sp * sr;
			x = cy * cp * sr - sy * sp * cr;
			y = sy * cp * sr + cy * sp * cr;
			z = sy * cp * cr - cy * sp * sr;
		}

		inline Vector<T,3> toEuler() const {
			Vector<T, 3> euler;

			// Roll (x-axis rotation)
			euler.x = std::atan2(2 * (w*x + y*z), w*w - x*x - y*y + z*z);

			// Pitch (y-axis rotation)
			euler.y = std::asin(2 * (w * y - z * x));

			// Yaw (z-axis rotation)
			euler.z = std::atan2(2 * (w * z + x * y), w * w + x * x - y * y - z * z);
			return euler;
		}

		inline T magnitude() const {
			return std::sqrt(x * x + y * y + z * z + w * w);
		}

		void normalize() {
			T mag = magnitude();
			if (mag > 0.0) {
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

		inline Vector<T, 3> globalToLocal(const Vector<T, 3>& v) const {
			Math::Quaternion vectorQuaternion(0.f, v);
			auto rotatedVector = inverseQuaternion() * vectorQuaternion * *this;
			return { rotatedVector.x, rotatedVector.y, rotatedVector.z };
		}

		inline Vector<T, 3> rotateVector(const Vector<T, 3>& v) const {
			Math::Quaternion vectorQuaternion(0.f, v);
			auto rotatedVector = *this * vectorQuaternion * inverseQuaternion();
			return { rotatedVector.x, rotatedVector.y, rotatedVector.z };
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

		Quaternion operator+=(const Vector<T, 3>& vec) {
			Quaternion q(0, vec.x, vec.y, vec.z);
			q = q * *this;
			x += q.x * 0.5f;
			y += q.y * 0.5f;
			z += q.z * 0.5f;
			w += q.w * 0.5f;

			normalize();

			return *this;
		}

		constexpr Quaternion(){}

		constexpr Quaternion(T x, T y, T z) {
			eulerToQuaternion(x, y, z);
		}

		constexpr Quaternion(T w, T x, T y, T z) : w{ w }, x{ x }, y{ y }, z{ z } {}

		constexpr Quaternion(T w, const Vector<T,3>& vec ) : w{ w }, x{ vec.x }, y{ vec.y }, z{ vec.z } {}

		constexpr Quaternion(const Vector<T, 3>& direction, const Vector<T, 3>& axis) {
			auto quaternionAxis = cross(axis, direction);
			quaternionAxis.normalize();

			auto angle = acos(dot(axis, direction)) * 0.5;
			auto s = sin(angle);

			w = cos(angle);
			x = quaternionAxis.x * s;
			y = quaternionAxis.y * s;
			z = quaternionAxis.z * s;
		}

	private:
		void matrixToQuaternionImpl(const Matrix<T,3,3>& matrix) {
			T trace = matrix[0].m[0] + matrix[1].m[1] + matrix[2].m[2];

			if (trace > 0) {
				T s = 0.5f / sqrt(trace + 1.0f);
				w = 0.25f / s;
				x = (matrix[2].m[1] - matrix[1].m[2]) * s;
				y = (matrix[0].m[2] - matrix[2].m[0]) * s;
				z = (matrix[1].m[0] - matrix[0].m[1]) * s;
			}
			else {
				if (matrix[0].m[0] > matrix[1].m[1] && matrix[0].m[0] > matrix[2].m[2]) {
					T s = 2.0f * sqrt(1.0f + matrix[0].m[0] - matrix[1].m[1] - matrix[2].m[2]);
					w = (matrix[2].m[1] - matrix[1].m[2]) / s;
					x = 0.25f * s;
					y = (matrix[0].m[1] + matrix[1].m[0]) / s;
					z = (matrix[0].m[2] + matrix[2].m[0]) / s;
				}
				else if (matrix[1].m[1] > matrix[2].m[2]) {
					T s = 2.0f * sqrt(1.0f + matrix[1].m[1] - matrix[0].m[0] - matrix[2].m[2]);
					w = (matrix[0].m[2] - matrix[2].m[0]) / s;
					x = (matrix[0].m[1] + matrix[1].m[0]) / s;
					y = 0.25f * s;
					z = (matrix[1].m[2] + matrix[2].m[1]) / s;
				}
				else {
					T s = 2.0f * sqrt(1.0f + matrix[2].m[2] - matrix[0].m[0] - matrix[1].m[1]);
					w = (matrix[1].m[0] - matrix[0].m[1]) / s;
					x = (matrix[0].m[2] + matrix[2].m[0]) / s;
					y = (matrix[1].m[2] + matrix[2].m[1]) / s;
					z = 0.25f * s;
				}
			}

			normalize();
		}
	};
	template <typename T>
	inline Quaternion<T> normalize(const Quaternion<T>& quat) {
		Quaternion<T> res = quat;
		T mag = res.magnitude();
		if (mag > 0.0) {
			res.x /= mag;
			res.y /= mag;
			res.z /= mag;
			res.w /= mag;
		}

		return res;
	}
	template <typename T, typename T1>
	inline Quaternion<T> slerp(const Quaternion<T>& q0, const Quaternion<T1>& q1, T inFraction) {
		auto sign_scale1 = 1.0f;
		auto cos_omega = std::max(-1.0f, std::min(1.0f, q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z));//dot product

		if (cos_omega < 0.0f) {
			cos_omega = -cos_omega;
			sign_scale1 = -sign_scale1;
		}

		float scale0, scale1;
		if (1.0f - cos_omega > 0.0001f) {
			float omega = acos(cos_omega);
			float sin_omega = sin(omega);
			scale0 = sin((1.0f - inFraction) * omega) / sin_omega;
			scale1 = sign_scale1 * sin(inFraction * omega) / sin_omega;
		}
		else {
			scale0 = 1.0f - inFraction;
			scale1 = sign_scale1 * inFraction;
		}

		auto vec = Vector<T,4>(scale0) * Vector<T, 4> { q0.x, q0.y, q0.z, q0.w } + Vector<T, 4>(scale1) * Vector<T, 4> { q1.x, q1.y, q1.z, q1.w };
		auto res = Quaternion<T>(vec.w, vec.x, vec.y, vec.z);
		res.normalize();
		return res;
	}
}
