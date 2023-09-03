#pragma once

#include "Core.h"
#include "InputHandler.h"
#include "Singleton.h"

#include "debugModule/DebugMenu.h"


class Camera;

namespace Engine {
	class UnnamedEngine : public Singleton<UnnamedEngine> {
		friend Singleton;
	public:
		void init() override;
		void update();

		float getDeltaTime() const;
		int getFPS() const;

		bool isAlive();

		GLFWwindow* getMainWindow();
		Camera* getCamera();
		RenderModule::Renderer* getRenderer() const;
	private:
		UnnamedEngine();
		~UnnamedEngine();
		void updateDelta();
		bool checkNeedClose();

		float lastFrame = 0.f;
		float deltaTime = 0.f;
		float fpsTimer = 0.f;
		int framesPerSecond = 0;
		int fps = 0;

		bool alive = true;

		CoreModule::Core* core = nullptr;
		RenderModule::Renderer* render = nullptr;

		GLFWwindow* window = nullptr;

		Camera* camera = nullptr;
		Debug::DebugMenu debugMenu;
	};
}

