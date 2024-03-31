#include "Engine.h"

#include <chrono>
#include <thread>

#include "Core.h"
#include "InputHandler.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Depth.h"
#include "glWrapper/Draw.h"
#include "glWrapper/ViewportStack.h"
#include "logsModule/logger.h"
#include "multithreading/ThreadPool.h"
#include "renderModule/TextRenderer.h"

namespace SFE {
	void Engine::setWindow(Render::Window* window) {
		if (mWindow) {
			delete mWindow;
		}
		mWindow = window;
		if (mWindow) {
			mWindow->onFramebufferResize = [](int width, int height) {
				SFE::ThreadPool::instance()->addTask<SFE::WorkerType::SYNC>([width, height]() {
					SFE::GLW::ViewportStack::size = { width, height };
				});
			};

			checkNeedClose();
		}
	}

	void Engine::initThread() {
		if (!mWindow) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Try to initialize thread without context, forgot to call createWindow?");
			return;
		}

		mMainThreadID = std::this_thread::get_id();
		glfwMakeContextCurrent(mWindow->getWindow());
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
		if(!mWindow) {
			SFE::LogsModule::Logger::LOG_FATAL(false, "Try to initialize render without context, forgot to call createWindow?");
			return;
		}

		SFE::GLW::ViewportStack::size = { mWindow->getScreenData().renderW, mWindow->getScreenData().renderH };
		glfwSwapInterval(0);

		SFE::GLW::setClearColor(0.f, 0.f, 0.f);

		GLW::CapabilitiesStack<SFE::GLW::CULL_FACE>::push(true);
		SFE::GLW::CapabilitiesStack<SFE::GLW::DEPTH_TEST>::push(true);
		SFE::GLW::DepthFuncStack::push(SFE::GLW::DepthFunc::LESS);

		SFE::GLW::setDepthDistance(mWindow->getScreenData().far);

		SFE::LogsModule::Logger::LOG_INFO("engine render initialized");
	}


	Render::Window* Engine::createWindow(int width, int height, GLFWwindow* window, const std::string& title, Render::WindowHints hints) {
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

		getWindow()->swapBuffers();

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

	Engine::~Engine() {
		destroyWindow();
	}

	void Engine::fpsSync(int fps) const {
		if (mDeltaTime < 1.f / fps) {
			std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>((1.f / fps - mDeltaTime) * 1000.f * 1000.f)));
		}
	}
}
