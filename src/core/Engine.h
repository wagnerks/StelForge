#pragma once

#include "Camera.h"
#include "Core.h"
#include "InputHandler.h"

#include "debugModule/DebugMenu.h"


namespace GameEngine {
	class Engine {
	public:
		static Engine* getInstance();
		static void terminate();

		void init();
		void update();

		float getDeltaTime() const;
		int getFPS() const;

		bool isAlive();
		
		GLFWwindow* getMainWindow();
		Camera& getCamera();
	private:

		void updateDelta();
		bool checkNeedClose();
		Engine();
		~Engine();
		inline static Engine* instance = nullptr;

		float lastFrame = 0.f;
		float deltaTime = 0.f;
		float fpsTimer = 0.f;
		int framesPerSecond = 0;
		int fps = 0;

		bool alive = true;

		CoreModule::Core* core = nullptr;
		RenderModule::Renderer* render = nullptr;

		GLFWwindow* window = nullptr;

		Camera camera;
		Debug::DebugMenu debugMenu;
	};
}

