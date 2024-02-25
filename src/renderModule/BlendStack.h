#pragma once

#include <stack>

#include "core/Singleton.h"
#include "glad/glad.h"

namespace SFE::Render {
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
		BlendFunction sourceFactor = ONE; //Specifies how the red, green, blue, and alpha source blending factors are computed
		BlendFunction destinationFactor = ZERO; //Specifies how the red, green, blue, and alpha destination blending factors are computed
	};

	struct BlendFuncStack : Singleton<BlendFuncStack> {//may be is should exist per opengl context, but now i have only one context todo
		void init() override {
			states.push({});
		}

		static void push(BlendFuncState blend) {
			instance()->pushImpl(std::move(blend));
		}

		static void pop() {
			instance()->popImpl();
		}

		void pushImpl(BlendFuncState blend) {
			auto& prev = states.top();
			
			if (blend.sourceFactor != prev.sourceFactor && blend.destinationFactor != prev.destinationFactor) {
				glBlendFunc(blend.sourceFactor, blend.destinationFactor);
			}

			states.emplace(std::move(blend));
		}

		void popImpl() {
			const auto currentBlend = states.top();
			states.pop();
			const auto& prev = states.top();
			if (currentBlend.sourceFactor != prev.sourceFactor && currentBlend.destinationFactor != prev.destinationFactor) {
				glBlendFunc(prev.sourceFactor, prev.destinationFactor);
			}
		}

	private:
		std::stack<BlendFuncState> states;
	};
}
