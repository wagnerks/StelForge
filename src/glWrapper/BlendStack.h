#pragma once

#include "Blend.h"
#include "StateStack.h"

namespace SFE::GLW {
	struct BlendFuncStack : StateStack<BlendFuncState, BlendFuncStack>{
		constexpr void apply(BlendFuncState* state) override {
			if (!state) {
				setBlendFunction({});
			}
			else {
				setBlendFunction(*state);
			}
		}
	};
}
