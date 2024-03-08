#pragma once
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/ScreenDrawData.h"
#include "core/Singleton.h"

namespace SFE::Render {

	class Window {
	public:
		static void init();

		static Window createWindow(int w, int h, const std::string& title, GLFWwindow* share);
		GLFWwindow* getWindow() { return mWindow; }
	private:
		inline static bool mGLFWInited = false;

		CoreModule::ScreenDrawData data;
		GLFWwindow* mWindow = nullptr;
	};

	class Renderer : public Singleton<Renderer> {
	public:
		~Renderer() override;

		void swapBuffer();

		inline static CoreModule::ScreenDrawData screenDrawData;

	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool mGLFWInited = false;
	};
}

