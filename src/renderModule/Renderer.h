#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/SceneFloor.h"
#include "debugModule/ComponentsDebug.h"
#include "modelModule/Model.h"

namespace GameEngine::RenderModule {
	class Renderer {
	public:
		void draw();
		void postDraw();
		void init();
		void terminate() const;
	private:
		NodeModule::Node* sceneNode = nullptr;
		ModelModule::Model* modelObj = nullptr;
		ModelModule::Model* modelObj2 = nullptr;
		SceneFloor floor;
	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool GLFWInited = false;
	};
}

