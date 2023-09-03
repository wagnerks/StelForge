#pragma once
#include "renderModule/Renderer.h"

namespace Engine::CoreModule {
	class InputHandler {
	public:
		static void processInput(GLFWwindow* window);
		static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
		static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void mouseBtnInput(GLFWwindow* w, int btn, int act, int mode);
		static void init();
	};
}

