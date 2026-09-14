#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <GLFW/glfw3.h>

#include "rhi/GraphicsBackend.h"
#include "rhi/RenderHardwareInterface.h"
#include "rhi/RhiLog.h"
#include "rhi/RhiFactory.h"

namespace SFE::Render::Tests {

	struct Rgba {
		uint8_t r = 0, g = 0, b = 0, a = 0;
		bool operator==(const Rgba& other) const {
			return r == other.r && g == other.g && b == other.b && a == other.a;
		}
	};

	// Reads one pixel out of a layer. Every assertion here is about a value the backend
	// produced, never about a screenshot, so the two backends can be held to the same number.
	inline bool readPixel(RenderHardwareInterface& rhi, TextureHandle tex, uint32_t layer, Rgba& out) {
		std::vector<uint8_t> pixels;
		int w = 0;
		int h = 0;
		if (!rhi.readbackTexture(tex, pixels, w, h, layer) || pixels.size() < 4) {
			return false;
		}
		out = { pixels[0], pixels[1], pixels[2], pixels[3] };
		return true;
	}

	inline TextureHandle makeColorArray(RenderHardwareInterface& rhi, uint32_t layers) {
		TextureDesc desc;
		desc.width = 4;
		desc.height = 4;
		desc.depth = layers;
		desc.arrayLayers = layers;
		desc.dimension = layers > 1 ? TextureDimension::Array : TextureDimension::Tex2D;
		desc.format = TextureFormat::RGBA8;
		desc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled | TextureUsage::TransferSrc;
		return rhi.createTexture(desc);
	}

	inline RenderPassHandle makeClearPass(RenderHardwareInterface& rhi, const Math::Vec4& clear) {
		RenderPassDesc desc;
		desc.colorAttachmentCount = 1;
		desc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
		desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		desc.colorAttachments[0].format = TextureFormat::RGBA8;
		desc.colorAttachments[0].clearColor = clear;
		return rhi.createRenderPass(desc);
	}

	inline FramebufferHandle makeLayerFramebuffer(RenderHardwareInterface& rhi, RenderPassHandle pass,
		TextureHandle tex, uint32_t layer) {
		FramebufferDesc desc;
		desc.width = 4;
		desc.height = 4;
		desc.layers = 1;
		desc.renderPass = pass;
		FramebufferAttachmentDesc att;
		att.texture = tex;
		att.mipLevel = 0;
		att.layer = layer;
		desc.colorAttachments = { att };
		return rhi.createFramebuffer(desc);
	}

	inline void runClearPass(RenderHardwareInterface& rhi, RenderPassHandle pass, FramebufferHandle fb) {
		// Command lists live inside a frame: that is where the Vulkan backend acquires its
		// per-frame command pool and fences, and where OpenGL resets its bound state.
		rhi.beginFrame();
		const auto cmd = rhi.beginCommandList(CommandListType::Graphics);
		RenderPassBeginInfo begin;
		begin.renderPass = pass;
		begin.framebuffer = fb;
		begin.width = 4;
		begin.height = 4;
		rhi.cmdBeginRenderPass(cmd, begin);
		rhi.cmdEndRenderPass(cmd);
		rhi.endCommandList(cmd);
		// Recording is not execution: the OpenGL backend replays the recorded list here, and
		// Vulkan hands it to the queue. Leaving this out is why the first cut of these tests
		// read back nothing but zeros.
		rhi.submit(cmd);
		// Closes the frame opened above. Vulkan resets its per-frame fences and query pool
		// here, so skipping it makes the second frame submit an already signalled fence.
		rhi.present();
		rhi.waitIdle();
	}

	// The whole point of the suite: an RHI standing on its own, with no engine behind it. The
	// backend used to call Engine::instance() for the window and the present flags, so a test
	// could not create one without booting everything; RhiHost is that dependency made explicit,
	// and here it is answered by a hidden window and "present nothing".
	class TestRhiHost final : public RhiHost {
	public:
		explicit TestRhiHost(GLFWwindow* window) : mWindow(window) {}
		RhiSurface surface() const override {
			RhiSurface surface;
			surface.window = mWindow;
			if (surface.window) {
				glfwGetWindowSize(surface.window, &surface.width, &surface.height);
				glfwGetFramebufferSize(surface.window, &surface.backbufferWidth, &surface.backbufferHeight);
				surface.renderWidth = surface.width;
				surface.renderHeight = surface.height;
			}
			return surface;
		}
		// A test renders to its own targets and reads them back. Presenting would need a swapchain
		// and a visible window, and would tell us nothing about the calls under test.
		bool presentToScreen() const override { return false; }

		// The suite's own reader, so shader loading needs nothing from the engine's asset system:
		// paths are relative to the working directory (engine/bin), and #include "x" resolves next to
		// the including file, then under shaders/.
		std::string loadShaderSource(std::string_view path) const override {
			std::unordered_set<std::string> seen;
			return expand(std::filesystem::path(path), seen);
		}
		std::filesystem::path shaderCacheDir() const override { return "shaders/vulkan"; }

	private:
		static std::string expand(const std::filesystem::path& path, std::unordered_set<std::string>& seen) {
			if (!seen.insert(path.lexically_normal().generic_string()).second) {
				return {};
			}
			std::ifstream in(path, std::ios::binary);
			if (!in) {
				return {};
			}
			const std::string text(std::istreambuf_iterator<char>(in), {});
			std::string out;
			size_t pos = 0;
			while (pos < text.size()) {
				size_t end = text.find('\n', pos);
				end = end == std::string::npos ? text.size() : end + 1;
				const std::string_view line(text.data() + pos, end - pos);
				const auto directive = line.find("#include");
				const auto open = line.find('"');
				const auto close = open == std::string_view::npos ? open : line.find('"', open + 1);
				if (directive != std::string_view::npos && line.find_first_not_of(" \t") == directive && close != std::string_view::npos) {
					const std::filesystem::path name(line.substr(open + 1, close - open - 1));
					auto resolved = path.parent_path() / name;
					if (!std::filesystem::exists(resolved)) {
						resolved = std::filesystem::path("shaders") / name;
					}
					out += expand(resolved, seen);
				}
				else {
					out.append(line);
				}
				pos = end;
			}
			return out;
		}


		GLFWwindow* mWindow = nullptr;
	};

	// Everything the RHI logged, installed as its log sink by main(). A test fails on any Vulkan
	// validation error it caused (the backend logs those as Fatal). Validation warnings -- a vertex
	// attribute the shader does not read -- and the RHI's own errors are left to the tests, since
	// several provoke them on purpose.
	struct RhiLogCapture {
		std::mutex mutex;
		std::vector<std::pair<RhiLogLevel, std::string>> messages;
	};
	inline RhiLogCapture& rhiLogCapture() {
		static RhiLogCapture capture;
		return capture;
	}
	inline void captureRhiLog(const RhiLogLevel level, const char* message) {
		auto& capture = rhiLogCapture();
		std::lock_guard lock(capture.mutex);
		capture.messages.emplace_back(level, message);
	}

	// GLFW is brought up once for the whole process rather than once per test. Cycling
	// glfwInit/glfwTerminate around every test made Vulkan's device creation fault intermittently a
	// couple of dozen tests into a run: terminating unloads the Vulkan loader that the next test's
	// instance is about to use. The failures landed on whichever tests happened to run last, which
	// made them look like leaks in unrelated code.
	inline bool& glfwReady() {
		static bool ready = false;
		return ready;
	}

	class GlfwEnvironment final : public ::testing::Environment {
	public:
		void SetUp() override { glfwReady() = glfwInit() == GLFW_TRUE; }
		void TearDown() override {
			if (glfwReady()) {
				glfwTerminate();
				glfwReady() = false;
			}
		}
	};

	// One fixture, both backends: the same test body runs twice and every expectation is written
	// once. A convention that holds on one backend and not the other fails here by name instead of
	// showing up later as "the frame differs by ten percent".
	class RhiBackendTest : public ::testing::TestWithParam<GraphicsBackend> {
	public:
		// Names the instantiated cases "Backends/RhiBackendTest.Foo/OpenGL" instead of "/0", so a
		// failure says which backend broke without counting indices.
		static std::string backendName(const ::testing::TestParamInfo<GraphicsBackend>& info) {
			return info.param == GraphicsBackend::Vulkan ? "Vulkan" : "OpenGL";
		}

	protected:
		void SetUp() override {
			{
				auto& capture = rhiLogCapture();
				std::lock_guard lock(capture.mutex);
				capture.messages.clear();
			}
			if (!glfwReady()) {
				GTEST_SKIP() << "glfwInit failed: no display";
			}

			// Hidden, and for Vulkan without a client API: it owns its own surface and must not be
			// handed a GL context.
			glfwDefaultWindowHints();
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
			if (GetParam() == GraphicsBackend::Vulkan) {
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			}
			mWindow = glfwCreateWindow(64, 64, "rhi tests", nullptr, nullptr);
			glfwDefaultWindowHints();
			if (!mWindow) {
				GTEST_SKIP() << "window creation failed";
			}

			mHost = std::make_unique<TestRhiHost>(mWindow);
			if (GetParam() == GraphicsBackend::OpenGL) {
				glfwMakeContextCurrent(mWindow);
			}
			mRhi = createRhi(GetParam());
			mRhi->setHost(mHost.get());
			mRhi->init();
		}

		void TearDown() override {
			if (mRhi) {
				mRhi->waitIdle();
				mRhi->cleanup();
				mRhi.reset();
			}
			mHost.reset();
			// Release the GL context before its window dies. Leaving it current leaves the GL driver's
			// thread state behind, and the next Vulkan test creates its device on that thread.
			glfwMakeContextCurrent(nullptr);
			if (mWindow) {
				glfwDestroyWindow(mWindow);
				mWindow = nullptr;
			}

			auto& capture = rhiLogCapture();
			std::lock_guard lock(capture.mutex);
			for (const auto& [level, message] : capture.messages) {
				if (level == RhiLogLevel::Fatal && message.rfind("Vulkan validation", 0) == 0) {
					ADD_FAILURE() << message;
				}
			}
		}

		RenderHardwareInterface& rhi() { return *mRhi; }


	private:
		GLFWwindow* mWindow = nullptr;
		std::unique_ptr<TestRhiHost> mHost;
		std::unique_ptr<RenderHardwareInterface> mRhi;
	};
}
