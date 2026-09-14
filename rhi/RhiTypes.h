#pragma once

// Small value types the RHI interface uses, with no dependency on any math library.
//
// A host extends them with conversions to its own types, the way ImGui's IM_VEC4_CLASS_EXTRA works:
// define SFE_RHI_USER_CONFIG as the path of a header (a compile definition, e.g.
// SFE_RHI_USER_CONFIG="core/RhiConfig.h"), and in that header define SFE_RHI_VEC4_EXTRA as extra
// members of RhiVec4 -- typically a constructor from the host's vector and a conversion back. Without a
// config the types are plain structs.
#ifdef SFE_RHI_USER_CONFIG
#include SFE_RHI_USER_CONFIG
#endif

namespace SFE::Render {
	struct RhiVec4 {
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
		float w = 0.f;

		constexpr RhiVec4() = default;
		constexpr RhiVec4(const float x_, const float y_, const float z_, const float w_) : x(x_), y(y_), z(z_), w(w_) {}

#ifdef SFE_RHI_VEC4_EXTRA
		SFE_RHI_VEC4_EXTRA
#endif
	};
	// Arrays of RhiVec4 are handed to the backends as raw float data.
	static_assert(sizeof(RhiVec4) == 4 * sizeof(float));
}
