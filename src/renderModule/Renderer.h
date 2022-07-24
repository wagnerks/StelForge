#pragma once

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "DirectionalLight.h"

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
		unsigned int uboMatrices;
		NodeModule::Node* sceneNode = nullptr;
		ModelModule::Model* modelObj = nullptr;
		std::vector<LightsModule::DirectionalLight*> lights;

		unsigned int depthCubemap;
		unsigned int buffer;
		
	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool GLFWInited = false;
	};
}

