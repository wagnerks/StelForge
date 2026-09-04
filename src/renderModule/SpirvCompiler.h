#pragma once

#include <string_view>

#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	// GLSL → shaders/vulkan/<name>.spv via in-process shaderc (no glslc/cmd).
	// `shaderFileName` is the basename, e.g. "g_buffer.vs".
	bool ensureSpirvUpToDate(ShaderStage stage, std::string_view shaderFileName);
	// Recompile every cached SPIR-V whose GLSL (or sfe.glsl) is newer. Shows window-title progress.
	void compileDirtySpirv();
}
