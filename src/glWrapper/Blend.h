#pragma once

#include "glad/glad.h"

namespace SFE::GLW {
	//https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlendFunc.xhtml
	enum BlendFunction {
		ZERO = GL_ZERO,
		ONE = GL_ONE,
		SRC_COLOR = GL_SRC_COLOR,
		ONE_MINUS_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,
		DST_COLOR = GL_DST_COLOR,
		ONE_MINUS_DST_COLOR = GL_ONE_MINUS_DST_COLOR,
		SRC_ALPHA = GL_SRC_ALPHA,
		ONE_MINUS_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA,
		DST_ALPHA = GL_DST_ALPHA,
		ONE_MINUS_DST_ALPHA = GL_ONE_MINUS_DST_ALPHA,
		CONSTANT_COLOR = GL_CONSTANT_COLOR,
		ONE_MINUS_CONSTANT_COLOR = GL_ONE_MINUS_CONSTANT_COLOR,
		CONSTANT_ALPHA = GL_CONSTANT_ALPHA,
		ONE_MINUS_CONSTANT_ALPHA = GL_ONE_MINUS_CONSTANT_ALPHA
	};

	struct BlendFuncState {
		constexpr friend bool operator==(const BlendFuncState& lhs, const BlendFuncState& rhs) {
			return lhs.sourceFactor == rhs.sourceFactor && lhs.destinationFactor == rhs.destinationFactor;
		}

		constexpr friend bool operator!=(const BlendFuncState& lhs, const BlendFuncState& rhs) {
			return !(lhs == rhs);
		}

		BlendFunction sourceFactor = ONE; //Specifies how the red, green, blue, and alpha source blending factors are computed
		BlendFunction destinationFactor = ZERO; //Specifies how the red, green, blue, and alpha destination blending factors are computed
	};

	constexpr inline void setBlendFunction(BlendFunction sourceFactor, BlendFunction destinationFactor) {
		glBlendFunc(sourceFactor, destinationFactor);
	}

	constexpr inline void setBlendFunction(BlendFuncState blendFuncState) {
		glBlendFunc(blendFuncState.sourceFactor, blendFuncState.destinationFactor);
	}
}
