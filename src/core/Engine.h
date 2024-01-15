#pragma once

#include <thread>

#include "Core.h"
#include "InputHandler.h"
#include "Singleton.h"

class Camera;

namespace Engine {
	class UnnamedEngine : public Singleton<UnnamedEngine>, public CoreModule::InputObserver {
		friend Singleton;
	public:
		void init() override;
		void update();

		float getDeltaTime() const;
		int getFPS() const;

		bool isAlive() const;

		GLFWwindow* getMainWindow() const;
		GLFWwindow* getEditorWindow();

		static bool isMainThread();

		int maxFPS = 60;
	private:
		UnnamedEngine() = default;
		~UnnamedEngine() override;
		void updateDelta();
		void checkNeedClose();

		float mLastFrame = 0.f;
		float mDeltaTime = 0.f;
		float mFramesTimer = 0.f;
		int mFramesCounter = 0;
		int mFPS = 0;


		bool mAlive = false;

		CoreModule::Core* mCore = nullptr;

		GLFWwindow* mMainWindow = nullptr;
		GLFWwindow* mEditorWindow = nullptr;
		void editorUpdate();
		inline static std::thread::id mMainThreadID;
	};
}

