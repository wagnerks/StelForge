#pragma once

#include <thread>

#include "Core.h"
#include "InputHandler.h"
#include "containersModule/Singleton.h"

class Camera;

namespace SFE {
	class Engine : public Singleton<Engine>, public CoreModule::InputObserver {
		friend Singleton;

	public:
		Render::Window* createWindow(int width, int height, GLFWwindow* window = nullptr, const std::string& title = "Engine", Render::WindowHints hints = {});
		void setWindow(Render::Window* window);
		void destroyWindow();

		void initThread();
		void initRender();

		void update();

		float getDeltaTime() const;
		int getFPS() const;

		bool isAlive() const;

		GLFWwindow* getMainWindow() const;
		Render::Window* getWindow() const;
		static bool isMainThread();

		int maxFPS = 60;

	protected:
		Engine() = default;
		~Engine() override;

	private:
		void fpsSync(int fps) const;
		void updateDelta();
		bool checkNeedClose();

		float mLastFrame = 0.f;
		float mDeltaTime = 0.f;
		float mFramesTimer = 0.f;
		int mFramesCounter = 0;
		int mFPS = 0;


		bool mAlive = false;

		CoreModule::Core mCore;
		
		Render::Window* mWindow = nullptr;
		inline static std::thread::id mMainThreadID;
	};
}

