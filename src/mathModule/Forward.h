#pragma once

#include "Matrix.h"
#include "Vector.h"

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
}
