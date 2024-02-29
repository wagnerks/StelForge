#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/ScreenDrawData.h"
#include "core/Singleton.h"

namespace SFE::Render {
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

