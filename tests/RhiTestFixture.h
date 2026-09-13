#pragma once

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <GLFW/glfw3.h>

#include "renderModule/GraphicsBackend.h"
#include "renderModule/OpenGLRHI.h"
#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/VulkanRHI.h"
#include "windowsModule/Window.h"

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
		explicit TestRhiHost(Window* window) : mWindow(window) {}
		Window* window() const override { return mWindow; }
		// A test renders to its own targets and reads them back. Presenting would need a swapchain
		// and a visible window, and would tell us nothing about the calls under test.
		bool presentToScreen() const override { return false; }
		bool publishEditorDisplay() const override { return false; }

	private:
		Window* mWindow = nullptr;
	};

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
			if (!glfwReady()) {
				GTEST_SKIP() << "glfwInit failed: no display";
			}

			// Hidden, and for Vulkan without a client API: it owns its own surface and must not be
			// handed a GL context.
			WindowHints hints = GetParam() == GraphicsBackend::Vulkan
				? WindowHints({ { VISIBLE, false } }, { { CLIENT_API, GLFW_NO_API } })
				: WindowHints({ { VISIBLE, false } });
			mWindow = std::make_unique<Window>(64, 64, "rhi tests", nullptr, hints);
			if (!mWindow->getWindow()) {
				GTEST_SKIP() << "window creation failed";
			}

			mHost = std::make_unique<TestRhiHost>(mWindow.get());
			if (GetParam() == GraphicsBackend::Vulkan) {
				mRhi = std::make_unique<VulkanRHI>();
			}
			else {
				glfwMakeContextCurrent(mWindow->getWindow());
				mRhi = std::make_unique<OpenGLRHI>();
			}
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
			mWindow.reset();
		}

		RenderHardwareInterface& rhi() { return *mRhi; }


	private:
		std::unique_ptr<Window> mWindow;
		std::unique_ptr<TestRhiHost> mHost;
		std::unique_ptr<RenderHardwareInterface> mRhi;
	};
}
