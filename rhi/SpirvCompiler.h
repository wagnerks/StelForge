#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	// GLSL -> SPIR-V with in-process shaderc. `source` is include-expanded GLSL; `name` is only used for
	// the cache file name and messages. With a cache directory, <cacheDir>/<basename>.spv is reused while
	// its .hash sidecar matches the source, so any change to the text -- an included file too -- rebuilds
	// it. An empty directory compiles every time. `beforeCompile` runs only when a compile actually
	// happens. Returns an empty vector on failure: never the previous, stale binary.
	std::vector<uint32_t> compileSpirv(ShaderStage stage, std::string_view name, const std::string& source,
		const std::filesystem::path& cacheDir, const std::function<void()>& beforeCompile = {});
}
