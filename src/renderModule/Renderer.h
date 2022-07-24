#pragma once

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "DirectionalLight.h"
#include "SceneGridFloor.h"
#include "Skybox.h"
#include "core/Scene.h"

namespace GameEngine {
	namespace ModelModule {
		class Model;
	}
}

namespace GameEngine {
	namespace NodeModule {
		class Node;
	}
}

namespace GameEngine::RenderModule {
	class Renderer {
	public:
		void draw();
		void postDraw();
		void init();
		void terminate() const;

		void drawCall();
		inline static int SCR_WIDTH = 1920;
		inline static int SCR_HEIGHT = 1080;
		inline static size_t drawCallsCount = 0;
		inline static size_t drawVerticesCount = 0;
	private:
		GameModule::CoreModule::Scene* scene = nullptr;
		
	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool GLFWInited = false;
	};
}

