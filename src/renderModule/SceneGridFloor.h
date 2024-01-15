#pragma once

#include "assetsModule/shaderModule/ShaderBase.h"

namespace Engine::RenderModule {
	class SceneGridFloor {
	public:
		SceneGridFloor();
		~SceneGridFloor();
		
		void draw();
	private:
		void init();
		Engine::ShaderModule::ShaderBase* floorShader = nullptr;
		size_t floorShaderHash;
		unsigned VAO = 0;
		unsigned VBO = 0;
		Math::Mat4 transform = Math::Mat4(1.f);
	};
}

