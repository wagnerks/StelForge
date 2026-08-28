#pragma once

#include <thread>

#include "Core.h"
#include "InputHandler.h"
#include "containersModule/Singleton.h"
#include "renderModule/RenderHardwareInterface.h"

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

		int maxFPS = 600;

		Render::RenderHardwareInterface* rhi() const { return mRHI; }

		void setEmbeddedInEditor(bool embedded) { mEmbeddedInEditor = embedded; }
		bool isEmbeddedInEditor() const { return mEmbeddedInEditor; }
		void setPublishEditorDisplay(bool publish) { mPublishEditorDisplay = publish; }
		bool publishEditorDisplay() const { return mPublishEditorDisplay; }
		void setPresentToScreen(bool present) { mPresentToScreen = present; }
		bool presentToScreen() const { return mPresentToScreen; }

	protected:
		Engine();
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
		bool mEmbeddedInEditor = false;
		bool mPublishEditorDisplay = false;
		bool mPresentToScreen = false;

		CoreModule::Core mCore;
		
		Render::Window* mWindow = nullptr;
		inline static std::thread::id mMainThreadID;

		Render::RenderHardwareInterface* mRHI = nullptr;
	};
}

