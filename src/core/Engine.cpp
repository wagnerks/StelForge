#include "Engine.h"

#include <chrono>
#include <thread>

#include <GLFW/glfw3.h>

#include "Core.h"
#include "InputHandler.h"
#include "logsModule/logger.h"
#include "multithreading/ThreadPool.h"
#include "renderModule/GraphicsBackend.h"
#include "renderModule/OpenGLRHI.h"
#include "renderModule/VulkanRHI.h"
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
			mRHI->init();
		}
		if (mRHI && mRHI->supportsFullPipeline()) {
			ThreadPool::instance()->ensureLoadingContext();
		}
		mCore.init();

		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type != CoreModule::InputEventType::PRESS) {
				return;
			}

			if (key == CoreModule::InputKey::KEY_ESCAPE) {
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
		const auto currentFrame = static_cast<float>(glfwGetTime());
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
		switch (activeGraphicsBackend()) {
		case GraphicsBackend::Vulkan:
			mRHI = new Render::VulkanRHI();
			break;
		default:
			mRHI = new Render::OpenGLRHI();
			break;
		}
		LogsModule::Logger::LOG_INFO("Engine backend: %s", graphicsBackendName(activeGraphicsBackend()));
	}

	Engine::~Engine() {
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
}
