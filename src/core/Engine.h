#pragma once

#include <thread>

#include "Core.h"
#include "InputHandler.h"
#include "Singleton.h"

class Camera;

namespace SFE {
	class Engine : public Singleton<Engine>, public CoreModule::InputObserver {
		friend Singleton;
	public:
		void init() override;
		void update();

		float getDeltaTime() const;
		int getFPS() const;

		bool isAlive() const;

		GLFWwindow* getMainWindow() const;

		static bool isMainThread();

		int maxFPS = 60;
	private:
		Engine() = default;
		~Engine() override;
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

		inline static std::thread::id mMainThreadID;
	};
}

