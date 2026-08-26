#pragma once

#include <string>

#include "mathModule/Forward.h"

namespace SFE::ShaderModule {
	class ShaderBase {
	public:

		static std::string loadShaderCode(std::string_view path);
	};

}
