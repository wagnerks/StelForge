#pragma once
#include "shaderModule/Shader.h"

namespace GameEngine::RenderModule {
	class Skybox {
	public:
		Skybox(std::string_view path);
		void init();
		void draw();
	private:
		ShaderModule::ShaderBase* skyboxShader = nullptr;
		unsigned VAO = 0;
		unsigned cubemapTex = 0;
		std::string skyboxPath;
	};
}
