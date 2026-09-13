#include "SpirvCompiler.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <shaderc/shaderc.hpp>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "core/FileSystem.h"
#include "logsModule/logger.h"
#include "windowsModule/Window.h"

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

		ShaderStage stageFromName(const std::string_view name) {
			if (name.ends_with(".vs")) {
				return ShaderStage::Vertex;
			}
			if (name.ends_with(".fs")) {
				return ShaderStage::Fragment;
			}
			if (name.ends_with(".gs")) {
				return ShaderStage::Geometry;
			}
			if (name.ends_with(".cs")) {
				return ShaderStage::Compute;
			}
			if (name.ends_with(".tesc")) {
				return ShaderStage::TessControl;
			}
			if (name.ends_with(".tese")) {
				return ShaderStage::TessEval;
			}
			return ShaderStage::Vertex;
		}

		std::filesystem::path sourcePathFor(const std::string_view fileName) {
			const auto common = FileSystem::resolve((std::filesystem::path("shaders") / std::string(fileName)).generic_string());
			if (std::filesystem::exists(common)) {
				return common;
			}
			return FileSystem::resolve((std::filesystem::path("shaders") / "vulkan" / std::string(fileName)).generic_string());
		}

		std::filesystem::path spvPathFor(const std::string_view fileName) {
			return FileSystem::resolve((std::filesystem::path("shaders") / "vulkan" / (std::string(fileName) + ".spv")).generic_string());
		}

		bool sourceNewerThanSpv(const std::filesystem::path& src, const std::filesystem::path& dst) {
			std::error_code ec;
			if (!std::filesystem::exists(dst, ec)) {
				return true;
			}
			const auto dstTime = std::filesystem::last_write_time(dst, ec);
			if (ec) {
				return true;
			}
			auto newer = [&](const std::filesystem::path& p) {
				if (!std::filesystem::exists(p, ec)) {
					return false;
				}
				return std::filesystem::last_write_time(p, ec) > dstTime;
			};
			return newer(src) || newer(FileSystem::resolve("shaders/sfe.glsl"));
		}

		void reportProgress(const int done, const int total, const std::string_view name) {
			LogsModule::Logger::LOG_INFO("SPIR-V %d/%d %s", done, total, std::string(name).c_str());
			// isAlive first: Singleton::instance() constructs on demand, so asking for the engine
			// merely to put progress in a window title would build a whole engine behind anyone who
			// compiles a shader without one. A test does exactly that.
			if (!Singleton<Engine>::isAlive()) {
				return;
			}
			auto* engine = Engine::instance();
			auto* window = engine ? engine->getWindow() : nullptr;
			GLFWwindow* glfw = window ? window->getWindow() : nullptr;
			if (!glfw) {
				return;
			}
			char title[256];
			std::snprintf(title, sizeof(title), "%s  —  shaders %d/%d  %s",
				window->title().c_str(), done, total, std::string(name).c_str());
			glfwSetWindowTitle(glfw, title);
			if (engine->presentToScreen()) {
				glfwPollEvents();
			}
		}

		shaderc::CompileOptions makeOptions() {
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			options.SetOptimizationLevel(shaderc_optimization_level_zero);
			// No AddMacroDefinition("VULKAN", ...) here: targeting Vulkan makes glslang predefine
			// VULKAN itself (as 100), and adding it again with a different value fails the compile
			// with "Macro redefined". That failure is non-fatal, so every shader silently kept its
			// last good .spv and Vulkan drifted away from OpenGL. sfe.glsl only tests #ifdef VULKAN,
			// so the predefined value is all it needs.
			return options;
		}

		bool compileToSpv(const ShaderStage stage, const std::filesystem::path& src, const std::filesystem::path& dst) {
			const auto source = ShaderModule::ShaderBase::loadShaderCode(src.generic_string());
			if (source.empty()) {
				LogsModule::Logger::LOG_ERROR("SPIR-V: empty source %s", src.string().c_str());
				return false;
			}

			shaderc::Compiler compiler;
			const auto options = makeOptions();
			const auto srcName = src.generic_string();
			const auto result = compiler.CompileGlslToSpv(
				source, kindFromStage(stage), srcName.c_str(), options);

			if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
				LogsModule::Logger::LOG_ERROR("SPIR-V compile failed (%s): %s",
					src.string().c_str(), result.GetErrorMessage().c_str());
				return false;
			}

			std::error_code ec;
			std::filesystem::create_directories(dst.parent_path(), ec);
			std::ofstream out(dst, std::ios::binary | std::ios::trunc);
			if (!out) {
				LogsModule::Logger::LOG_ERROR("SPIR-V: cannot write %s", dst.string().c_str());
				return false;
			}
			const auto bytes = static_cast<std::streamsize>(
				(result.cend() - result.cbegin()) * static_cast<std::ptrdiff_t>(sizeof(uint32_t)));
			out.write(reinterpret_cast<const char*>(result.cbegin()), bytes);
			if (!out) {
				LogsModule::Logger::LOG_ERROR("SPIR-V: write failed %s", dst.string().c_str());
				return false;
			}
			LogsModule::Logger::LOG_INFO("SPIR-V compiled %s", dst.string().c_str());
			return true;
		}
	}

	bool ensureSpirvUpToDate(const ShaderStage stage, const std::string_view shaderFileName) {
		if (shaderFileName.empty()) {
			return false;
		}

		const std::filesystem::path src = sourcePathFor(shaderFileName);
		const std::filesystem::path dst = spvPathFor(shaderFileName);

		std::error_code ec;
		const bool srcExists = std::filesystem::exists(src, ec);
		const bool dstExists = std::filesystem::exists(dst, ec);
		if (srcExists && dstExists && !sourceNewerThanSpv(src, dst)) {
			return true;
		}
		if (!srcExists && dstExists) {
			return true;
		}
		if (!srcExists) {
			LogsModule::Logger::LOG_ERROR("SPIR-V: missing shader source %s", std::string(shaderFileName).c_str());
			return false;
		}

		reportProgress(1, 1, shaderFileName);
		const bool ok = compileToSpv(stage, src, dst);
		if (auto* window = Singleton<Engine>::isAlive() ? Engine::instance()->getWindow() : nullptr) {
			if (auto* glfw = window->getWindow()) {
				glfwSetWindowTitle(glfw, window->title().c_str());
			}
		}
		return ok;
	}

	void compileDirtySpirv() {
		std::error_code ec;
		const auto cacheDir = FileSystem::resolve("shaders/vulkan");
		if (!std::filesystem::exists(cacheDir, ec)) {
			return;
		}

		struct Job {
			std::string name;
			ShaderStage stage;
			std::filesystem::path src;
			std::filesystem::path dst;
		};
		std::vector<Job> jobs;
		for (const auto& entry : std::filesystem::directory_iterator(cacheDir, ec)) {
			if (!entry.is_regular_file(ec)) {
				continue;
			}
			const auto filename = entry.path().filename().string();
			constexpr std::string_view spvExt = ".spv";
			if (filename.size() <= spvExt.size() || !filename.ends_with(spvExt)) {
				continue;
			}
			const auto name = filename.substr(0, filename.size() - spvExt.size());
			const auto src = sourcePathFor(name);
			if (!std::filesystem::exists(src, ec)) {
				continue;
			}
			if (!sourceNewerThanSpv(src, entry.path())) {
				continue;
			}
			jobs.push_back({ name, stageFromName(name), src, entry.path() });
		}
		if (jobs.empty()) {
			return;
		}

		const int total = static_cast<int>(jobs.size());
		int done = 0;
		for (const auto& job : jobs) {
			++done;
			reportProgress(done, total, job.name);
			compileToSpv(job.stage, job.src, job.dst);
		}
		if (auto* window = Singleton<Engine>::isAlive() ? Engine::instance()->getWindow() : nullptr) {
			if (auto* glfw = window->getWindow()) {
				glfwSetWindowTitle(glfw, window->title().c_str());
			}
		}
	}
}
