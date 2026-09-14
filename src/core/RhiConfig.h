#pragma once

// The engine's extensions of the RHI value types (see rhi/RhiTypes.h): its math types convert
// to and from them implicitly, so engine code keeps passing Math::Vec4 where the RHI takes RhiVec4.

#include "mathModule/Forward.h"

#define SFE_RHI_VEC4_EXTRA                                                                              \
	constexpr RhiVec4(const ::SFE::Math::Vec4& v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {}              \
	constexpr operator ::SFE::Math::Vec4() const { return ::SFE::Math::Vec4(x, y, z, w); }
