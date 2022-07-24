#pragma once
#include "shaderModule/Shader.h"
#include "mat4x4.hpp"

namespace GameEngine::RenderModule {
	class SceneGridFloor {
	public:
		SceneGridFloor(float size);
		~SceneGridFloor();
		void init();
		void draw();
	private:
		GameEngine::ShaderModule::ShaderBase* floorShader = nullptr;
		unsigned VAO = 0;
		unsigned VBO;
		float size = 0;
		glm::mat4 transform = glm::mat4(1.f);
	};
}

