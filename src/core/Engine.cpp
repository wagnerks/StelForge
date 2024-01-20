#include "Engine.h"

#include "debugModule/imguiDecorator.h"
#include <chrono>
#include <thread>

#include "Core.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "InputHandler.h"
#include "ThreadPool.h"
#include "assetsModule/shaderModule/ShaderController.h"

namespace Engine {
	void UnnamedEngine::init() {
		mMainThreadID = std::this_thread::get_id();
		mMainWindow = RenderModule::Renderer::initGLFW();
		if (!mMainWindow) {
			return;
		}

		mAlive = true;

		mCore = new CoreModule::Core();
		mCore->init();

		Debug::ImGuiDecorator::init(getMainWindow());

		onKeyEvent = [this](CoreModule::InputKey key, CoreModule::InputEventType type) {
			if (type != CoreModule::InputEventType::PRESS) {
				return;
			}

			if (key == CoreModule::InputKey::KEY_ESCAPE) {
				glfwSetWindowShouldClose(mMainWindow, true);
			}
		};
	}

	void UnnamedEngine::update() {
		if (!mAlive) {
			return;
		}

		checkNeedClose();

		updateDelta();
		mCore->update(mDeltaTime);

		if (mDeltaTime < 1.f / maxFPS) {
			std::this_thread::sleep_for(std::chrono::milliseconds((int)((1.f / maxFPS - mDeltaTime) * 1000.f)));
		}
	}

	float UnnamedEngine::getDeltaTime() const {
		return mDeltaTime;
	}

	int UnnamedEngine::getFPS() const {
		return mFPS;
	}

	void UnnamedEngine::updateDelta() {
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

	bool UnnamedEngine::isAlive() const {
		return mAlive;
	}

	void UnnamedEngine::checkNeedClose() {
		mAlive = !glfwWindowShouldClose(mMainWindow);
	}


	GLFWwindow* UnnamedEngine::getMainWindow() const {
		return mMainWindow;
	}

	bool UnnamedEngine::isMainThread() {
		return mMainThreadID == std::this_thread::get_id();
	}

	UnnamedEngine::~UnnamedEngine() {
		delete mCore;
		glfwDestroyWindow(getMainWindow());
	}
}
