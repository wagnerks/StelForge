#include "Engine.h"

#include <chrono>
#include <thread>

#include "Core.h"
#include "InputHandler.h"

namespace SFE {
	void Engine::init() {
		mMainThreadID = std::this_thread::get_id();
		if (mMainWindow = Render::Renderer::initGLFW(); !mMainWindow) {
			return;
		}

		mAlive = true;

		mCore.init();

		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type != CoreModule::InputEventType::PRESS) {
				return;
			}

			if (key == CoreModule::InputKey::KEY_ESCAPE) {
				glfwSetWindowShouldClose(getMainWindow(), true);
			}
		};
	}

	void Engine::update() {
		if (!mAlive) {
			return;
		}

		checkNeedClose();

		updateDelta();
		mCore.update(mDeltaTime);

		if (mDeltaTime < 1.f / maxFPS) {
			std::this_thread::sleep_for(std::chrono::milliseconds((int)((1.f / maxFPS - mDeltaTime) * 1000.f)));
		}
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
		if (mFramesTimer > 1.f) {
			mFPS = mFramesCounter;
			mFramesCounter = 0;
			mFramesTimer = 0.f;
		}
	}

	bool Engine::isAlive() const {
		return mAlive;
	}

	void Engine::checkNeedClose() {
		mAlive = !glfwWindowShouldClose(getMainWindow());
	}


	GLFWwindow* Engine::getMainWindow() const {
		return mMainWindow;
	}

	bool Engine::isMainThread() {
		return mMainThreadID == std::this_thread::get_id();
	}

	Engine::~Engine() {
		glfwDestroyWindow(getMainWindow());
	}
}
