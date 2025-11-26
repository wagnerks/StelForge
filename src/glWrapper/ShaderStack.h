#pragma once

#include "Shader.h"
#include "StateStack.h"
#include "assetsModule/shaderModule/Shader.h"

namespace SFE::GLW {
	struct ShaderStack : StateStack<ShaderModule::ShaderBase*, ShaderStack> {
		constexpr void apply(ShaderModule::ShaderBase** state) override {
			if (!state) {
				useProgram(0);
			}
			else {
				(*state)->use();
			}
		}
	};
}
