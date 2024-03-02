#pragma once

#include "StateStack.h"
#include "glad/glad.h"

namespace SFE::GLW {

	enum class DepthFunc {
		NEVER  = GL_NEVER, //Never passes.
		LESS = GL_LESS, // Passes if the incoming depth value is less than the stored depth value.
		EQUAL = GL_EQUAL, // Passes if the incoming depth value is equal to the stored depth value.
		LEQUAL = GL_LEQUAL, // Passes if the incoming depth value is less than or equal to the stored depth value.
		GREATER = GL_GREATER, // Passes if the incoming depth value is greater than the stored depth value.
		NOTEQUAL = GL_NOTEQUAL, // Passes if the incoming depth value is not equal to the stored depth value.
		GEQUAL = GL_GEQUAL, // Passes if the incoming depth value is greater than or equal to the stored depth value.
		ALWAYS = GL_ALWAYS, // Always passes.
	};


	constexpr inline void setDepthFunc(DepthFunc func) {
		glDepthFunc(static_cast<GLenum>(func));
	}

	constexpr inline void setDepthDistance(float distance) {
		glClearDepth(distance);
	}

	struct DepthFuncStack : StateStack<DepthFunc, DepthFuncStack> {
		constexpr void apply(DepthFunc* state) override {
			if (!state) {
				return;
			}

			setDepthFunc(*state);
		}
	};
}
