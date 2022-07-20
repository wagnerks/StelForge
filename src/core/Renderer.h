#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Model.h"
#include "SceneFloor.h"

namespace GameEngine {
	namespace CoreModule {
		class Node;
	}
}

namespace GameEngine::Render {
	class Renderer {
	public:
		void draw();
		void postDraw();
		void init();
		void terminate() const;
	private:
		CoreModule::Node* sceneNode = nullptr;
		Model* modelObj = nullptr;
		Model* modelObj2 = nullptr;
		SceneFloor floor;
	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool GLFWInited = false;
	};
}

