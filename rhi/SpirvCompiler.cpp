#include "rhi/SpirvCompiler.h"

#include <cstdio>
#include <fstream>
#include <iterator>

#include <shaderc/shaderc.hpp>

#include "rhi/RhiLog.h"

namespace SFE::Render {
	namespace {
		shaderc_shader_kind kindFromStage(const ShaderStage stage) {
			switch (stage) {
			case ShaderStage::Vertex: return shaderc_glsl_vertex_shader;
			case ShaderStage::Fragment: return shaderc_glsl_fragment_shader;
			case ShaderStage::Geometry: return shaderc_glsl_geometry_shader;
			case ShaderStage::Compute: return shaderc_glsl_compute_shader;
			case ShaderStage::TessControl: return shaderc_glsl_tess_control_shader;
			case ShaderStage::TessEval: return shaderc_glsl_tess_evaluation_shader;
			default: return shaderc_glsl_infer_from_source;
			}
		}

		shaderc::CompileOptions makeOptions() {
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			options.SetOptimizationLevel(shaderc_optimization_level_zero);
			// No AddMacroDefinition("VULKAN", ...) here: targeting Vulkan makes glslang predefine
			// VULKAN itself (as 100), and adding it again with a different value fails the compile
			// with "Macro redefined". sfe.glsl only tests #ifdef VULKAN, so the predefined value is all
			// it needs.
			return options;
		}

		// Part of the cache key, so changing the compile options above invalidates every cached binary.
		constexpr std::string_view kOptionsKey = "vulkan1.2;O0";

		std::string hashOf(const std::string& source) {
			uint64_t hash = 14695981039346656037ull;
			const auto mix = [&hash](std::string_view bytes) {
				for (const char c : bytes) {
					hash ^= static_cast<unsigned char>(c);
					hash *= 1099511628211ull;
				}
			};
			mix(kOptionsKey);
			mix(source);
			char text[17];
			std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
			return text;
		}

		std::string readText(const std::filesystem::path& path) {
			std::ifstream in(path, std::ios::binary);
			return in ? std::string(std::istreambuf_iterator<char>(in), {}) : std::string{};
		}

		std::vector<uint32_t> readSpirv(const std::filesystem::path& path) {
			std::ifstream in(path, std::ios::binary | std::ios::ate);
			const auto size = in ? static_cast<std::streamoff>(in.tellg()) : 0;
			if (size <= 0 || size % 4 != 0) {
				return {};
			}
			std::vector<uint32_t> code(static_cast<size_t>(size / 4));
			in.seekg(0);
			in.read(reinterpret_cast<char*>(code.data()), size);
			return in ? code : std::vector<uint32_t>{};
		}

		void writeCache(const std::filesystem::path& spv, const std::filesystem::path& key,
			const std::vector<uint32_t>& code, const std::string& hash)
		{
			std::error_code ec;
			std::filesystem::create_directories(spv.parent_path(), ec);
			std::ofstream out(spv, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(code.data()), static_cast<std::streamsize>(code.size() * sizeof(uint32_t)));
			if (!out) {
				RhiLog::error("SPIR-V: cannot write %s", spv.string().c_str());
				return;
			}
			out.close();
			std::ofstream(key, std::ios::binary | std::ios::trunc) << hash;
		}
	}

	std::vector<uint32_t> compileSpirv(const ShaderStage stage, const std::string_view name, const std::string& source,
		const std::filesystem::path& cacheDir, const std::function<void()>& beforeCompile)
	{
		if (source.empty()) {
			RhiLog::error("SPIR-V: no source for %s", std::string(name).c_str());
			return {};
		}

		const auto hash = hashOf(source);
		std::filesystem::path spv;
		std::filesystem::path key;
		if (!cacheDir.empty()) {
			const auto base = std::filesystem::path(name).filename().string();
			spv = cacheDir / (base + ".spv");
			key = cacheDir / (base + ".spv.hash");
			if (readText(key) == hash) {
				if (auto cached = readSpirv(spv); !cached.empty()) {
					return cached;
				}
			}
		}

		if (beforeCompile) {
			beforeCompile();
		}
		shaderc::Compiler compiler;
		const std::string nameText(name);
		const auto result = compiler.CompileGlslToSpv(source, kindFromStage(stage), nameText.c_str(), makeOptions());
		if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
			RhiLog::error("SPIR-V compile failed (%s): %s", nameText.c_str(), result.GetErrorMessage().c_str());
			return {};
		}

		std::vector<uint32_t> code(result.cbegin(), result.cend());
		if (!cacheDir.empty()) {
			writeCache(spv, key, code, hash);
		}
		return code;
	}
}
