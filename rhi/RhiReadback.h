#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	// Channels of a float colour format, or 0 for a format that is not one. Missing channels read back as
	// G = B = 0, A = 1 on both backends.
	inline int floatFormatChannels(const TextureFormat format) {
		switch (format) {
		case TextureFormat::R16F:
		case TextureFormat::R32F: return 1;
		case TextureFormat::RG16F:
		case TextureFormat::RG32F: return 2;
		case TextureFormat::RGBA16F:
		case TextureFormat::RGBA32F: return 4;
		default: return 0;
		}
	}

	// IEEE 754 binary16 to float, exactly: every half is representable as a float.
	inline float halfToFloat(const uint16_t half) {
		const int sign = (half >> 15) & 1;
		const int exponent = (half >> 10) & 0x1F;
		const int mantissa = half & 0x3FF;
		float value = 0.f;
		if (exponent == 0) {
			value = std::ldexp(static_cast<float>(mantissa), -24);
		}
		else if (exponent == 31) {
			value = mantissa == 0 ? INFINITY : NAN;
		}
		else {
			value = std::ldexp(static_cast<float>(mantissa + 1024), exponent - 25);
		}
		return sign ? -value : value;
	}

	// A float readback converted to RGBA8 on the CPU, one rule for every backend. Letting the driver do it --
	// glReadPixels into bytes on OpenGL, a blit into an RGBA8 image on Vulkan -- rounded some values to
	// neighbouring bytes differently, so identical float targets dumped as different images.
	inline uint8_t floatToUnorm8(const float value) {
		if (!(value > 0.f)) {
			return 0; // negative, zero and NaN
		}
		return static_cast<uint8_t>(std::min(value, 1.f) * 255.f + 0.5f);
	}
}
