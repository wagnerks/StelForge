#pragma once

#include <cstddef>
#include <type_traits>

#include "Matrix.h"
#include "Quaternion.h"
#include "Vector.h"

// Conventions for the whole module:
// - matrices are column-major: m[column][row], Matrix<T, Rows, Columns>;
// - vectors are columns, transforms compose right to left: P * V * M, and translate/rotate/scale(m, ...) return m * X;
// - right-handed, camera looks down -Z; projections are RH, NO clip depth [-1, 1] unless named _ZO;
// - angles are radians, except functions and parameters that say Degrees;
// - Euler angles are (roll about X, pitch about Y, yaw about Z), applied X then Y then Z;
// - == on vectors and matrices is exact; compare with a tolerance explicitly where rounding matters.
namespace SFE::Math {
	using Mat3 = Matrix<float, 3, 3>;
	using Mat4 = Matrix<float, 4, 4>;

	using DMat3 = Matrix<double, 3, 3>;

	using Vec2 = Vector<float, 2>;
	using Vec3 = Vector<float, 3>;
	using Vec4 = Vector<float, 4>;

	using IVec2 = Vector<int, 2>;
	using IVec3 = Vector<int, 3>;
	using IVec4 = Vector<int, 4>;

	using DVec2 = Vector<double, 2>;
	using DVec3 = Vector<double, 3>;
	using DVec4 = Vector<double, 4>;

	using Quat = Quaternion<float>;

	// .x/.y/.z, .xyz and m[] share storage through anonymous unions: not standard C++, but defined behaviour on
	// MSVC, GCC and Clang. Uploads to the GPU rely on the same layout, so it is pinned here.
	static_assert(sizeof(Vec2) == 2 * sizeof(float) && sizeof(Vec3) == 3 * sizeof(float) && sizeof(Vec4) == 4 * sizeof(float));
	static_assert(sizeof(Mat3) == 9 * sizeof(float) && sizeof(Mat4) == 16 * sizeof(float));
	static_assert(offsetof(Vec4, x) == 0 && offsetof(Vec4, y) == sizeof(float) && offsetof(Vec4, w) == 3 * sizeof(float));
	static_assert(std::is_trivially_copyable_v<Vec3> && std::is_trivially_copyable_v<Vec4> && std::is_trivially_copyable_v<Mat4>);

	static_assert(std::is_trivially_copyable_v<Mat3>);
	static_assert(std::is_trivially_move_assignable_v<Mat3>);
	static_assert(std::is_trivially_copy_assignable_v<Mat3>);
	static_assert(std::is_trivially_destructible_v<Mat3>);
}
