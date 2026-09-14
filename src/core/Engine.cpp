#include "Engine.h"

#include <chrono>
#include <cstdio>
#include <thread>

#include <GLFW/glfw3.h>

#include "Core.h"
#include "InputHandler.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "logsModule/logger.h"
#include "multithreading/ThreadPool.h"
#include "rhi/GraphicsBackend.h"
#include "rhi/RhiLog.h"
#include "rhi/RhiFactory.h"
#include "renderModule/TextRenderer.h"

namespace SFE {
	using Render::activeGraphicsBackend;
	using Render::graphicsBackendName;
	using Render::GraphicsBackend;

	void Engine::setWindow(Render::Window* window) {
		if (mWindow) {
			delete mWindow;
		}
		mWindow = window;
		if (mWindow) {
			mWindow->onFramebufferResize = [](int width, int height) {
				SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([width, height]() {
					//ViewportStack::size = { width, height };
				});
			};

			checkNeedClose();
		}
	}

	void Engine::initThread() {
		if (!mWindow || !mWindow->getWindow()) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Try to initialize thread without context, forgot to call createWindow?");
			return;
		}

		mMainThreadID = std::this_thread::get_id();
		if (activeGraphicsBackend() == GraphicsBackend::OpenGL) {
			glfwMakeContextCurrent(mWindow->getWindow());
		}
		if (mRHI) {
			reproTrace("initThread rhi");
			mRHI->init();
			reproTrace("initThread rhi ok");
		}
		reproTrace("initThread core");
		mCore.init();
		reproTrace("initThread core ok");

		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type != CoreModule::InputEventType::PRESS) {
				return;
			}

			if (key == CoreModule::InputKey::KEY_ESCAPE && mPresentToScreen) {
				glfwSetWindowShouldClose(getMainWindow(), true);
			}
		};

		SFE::LogsModule::Logger::LOG_INFO("engine thread initialized");
	}

	void Engine::initRender() {
		if (activeGraphicsBackend() == GraphicsBackend::OpenGL) {
			glfwSwapInterval(0);
		}
		SFE::LogsModule::Logger::LOG_INFO("engine render initialized (%s backend)",
			graphicsBackendName(activeGraphicsBackend()));
	}


	Render::Window* Engine::createWindow(int width, int height, GLFWwindow* window, const std::string& title, Render::WindowHints hints) {
		if (activeGraphicsBackend() == GraphicsBackend::Vulkan) {
			hints.ints[Render::CLIENT_API] = GLFW_NO_API;
		}
		setWindow(new Render::Window(width, height, title, window, hints));

		return mWindow;
	}

	void Engine::destroyWindow() {
		delete mWindow;
		mWindow = nullptr;
	}

	void Engine::update() {
		if (!checkNeedClose()) {
			return;
		}

		updateDelta();

		mCore.update(mDeltaTime);
	
		glfwPollEvents();

		//getWindow()->swapBuffers();

		fpsSync(maxFPS);
	}

	float Engine::getDeltaTime() const {
		return mDeltaTime;
	}

	int Engine::getFPS() const {
		return mFPS;
	}

	void Engine::updateDelta() {
		// A capture steps at a fixed rate: wall-clock deltas make everything time-dependent
		// (TAA history, volumetrics) land differently on a faster or slower run, which would
		// show up as a backend difference when it is only a timing difference.
		const auto currentFrame = mCapturingFrame
			? mLastFrame + 1.f / 60.f
			: static_cast<float>(glfwGetTime());
		mDeltaTime = currentFrame - mLastFrame;
		mLastFrame = currentFrame;

		mFramesCounter++;
		mFramesTimer += mDeltaTime;
		if (mFramesTimer >= 1.f) {
			mFPS = mFramesCounter;
			mFramesCounter = 0;
			mFramesTimer = 0.f;
		}
	}

	bool Engine::isAlive() const {
		return mAlive;
	}

	bool Engine::checkNeedClose() {
		if (!mWindow || !mWindow->getWindow()) {
			mAlive = false;
			return false;
		}
		if (!mPresentToScreen) {
			mAlive = true;
			return true;
		}
		mAlive = !mWindow->isClosing();
		return mAlive;
	}


	GLFWwindow* Engine::getMainWindow() const {
		return mWindow->getWindow();
	}

	Render::Window* Engine::getWindow() const {
		return mWindow;
	}

	bool Engine::isMainThread() {
		return mMainThreadID == std::this_thread::get_id();
	}

	Engine::Engine()
	{
		Render::setRhiLogSink([](const Render::RhiLogLevel level, const char* message) {
			switch (level) {
			case Render::RhiLogLevel::Info: LogsModule::Logger::LOG_INFO("%s", message); break;
			case Render::RhiLogLevel::Warning: LogsModule::Logger::LOG_WARNING("%s", message); break;
			case Render::RhiLogLevel::Error: LogsModule::Logger::LOG_ERROR("%s", message); break;
			case Render::RhiLogLevel::Fatal: LogsModule::Logger::LOG_FATAL(false, "%s", message); break;
			}
		});
		mRHI = Render::createRhi(activeGraphicsBackend()).release();
		mRHI->setHost(&mRhiHost);
		LogsModule::Logger::LOG_INFO("Engine backend: %s", graphicsBackendName(activeGraphicsBackend()));
	}

	Engine::~Engine() {
		if (mRHI) {
			if (mWindow && mWindow->getWindow() && activeGraphicsBackend() == GraphicsBackend::OpenGL) {
				glfwMakeContextCurrent(mWindow->getWindow());
			}
			mRHI->waitIdle();
		}
		mCore.shutdown();
		if (ThreadPool::isAlive()) {
			ThreadPool::instance()->shutdown();
		}
		if (mRHI) {
			if (mWindow && mWindow->getWindow() && activeGraphicsBackend() == GraphicsBackend::OpenGL) {
				glfwMakeContextCurrent(mWindow->getWindow());
			}
			mRHI->cleanup();
			delete mRHI;
			mRHI = nullptr;
		}
		destroyWindow();
	}

	void Engine::fpsSync(int fps) const {
		if (fps <= 0) {
			return;
		}

		const double target = 1.0 / static_cast<double>(fps);
		const double start = static_cast<double>(mLastFrame);
		for (;;) {
			const double remain = target - (glfwGetTime() - start);
			if (remain <= 0.0) {
				break;
			}
			// Windows sleep_for(<2ms) often overshoots to 1–15ms and tanks FPS.
			if (remain > 0.002) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	Render::RhiSurface Engine::RhiHostAdapter::surface() const {
		Render::RhiSurface surface;
		auto* window = mEngine->getWindow();
		if (!window || !window->getWindow()) {
			return surface;
		}
		const auto& screen = window->getScreenData();
		surface.window = window->getWindow();
		surface.visible = window->isVisible();
		surface.width = screen.width;
		surface.height = screen.height;
		surface.renderWidth = screen.renderW;
		surface.renderHeight = screen.renderH;
		surface.backbufferWidth = screen.backbufferW;
		surface.backbufferHeight = screen.backbufferH;
		return surface;
	}

	std::string Engine::RhiHostAdapter::loadShaderSource(std::string_view path) const {
		return ShaderModule::ShaderBase::loadShaderCode(path);
	}

	std::filesystem::path Engine::RhiHostAdapter::shaderCacheDir() const {
		return FileSystem::resolve("shaders/vulkan");
	}

	// Log only. Retitling the window from here ran inside a frame, where the resulting window message
	// reached ImGui's GLFW hook mid-frame and crashed.
	void Engine::RhiHostAdapter::onShaderCompile(std::string_view name, bool finished) const {
		if (!finished) {
			LogsModule::Logger::LOG_INFO("compiling shader %s", std::string(name).c_str());
		}
	}
}
