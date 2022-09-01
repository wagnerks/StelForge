#pragma once

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Batcher.h"
#include "CascadeShadows.h"
#include "DirectionalOrthoLight.h"
#include "SceneGridFloor.h"
#include "Skybox.h"
#include "core/Scene.h"
#include "core/BoundingVolume.h"

namespace GameEngine {
	namespace EntitiesModule {
		class Model;
	}
}

namespace GameEngine {
	namespace ModelModule {
		class Model;
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
		Batcher* getBatcher() { return batcher;};

		static void drawArrays(GLenum mode, GLsizei size, GLint first = 0);
		static void drawElements(GLenum mode, GLsizei size, GLenum type, const void* place = nullptr);
		static void drawElementsInstanced(GLenum mode, GLsizei size, GLenum type, GLsizei instancesCount, const void* place = nullptr);
		static void drawArraysInstancing(GLenum mode, GLsizei size, GLsizei instancesCount, GLint first = 0);

		inline static int SCR_WIDTH = 1920;
		inline static int SCR_HEIGHT = 1080;
		inline static size_t drawCallsCount = 0;
		inline static size_t drawVerticesCount = 0;
	private:
		GameModule::CoreModule::Scene* scene = nullptr;
		std::vector<glm::vec3> lightPositions;
		std::vector<glm::vec3> lightColors;
		std::vector<glm::vec3> objectPositions;

		std::vector<glm::vec3> randomLightSpeeds;

		ModelModule::Model* modelObj = nullptr;

		EntitiesModule::Model* node = nullptr;

		std::vector<LightsModule::DirectionalOrthoLight*> lightsObj;

		Batcher* batcher = nullptr;

	public:
		static GLFWwindow* initGLFW();
	private:
		inline static bool GLFWInited = false;
	};
}

