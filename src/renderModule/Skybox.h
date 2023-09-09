#pragma once
#include "assetsModule/shaderModule/Shader.h"

namespace Engine::RenderModule {
	class Skybox {
	public:
		Skybox(std::string_view path);
		~Skybox();
		void init();
		void draw();
	private:
		ShaderModule::ShaderBase* skyboxShader = nullptr;
		unsigned VAO = -1;
		unsigned VBO = -1;
		unsigned cubemapTex = -1;
		std::string skyboxPath;
	};
}
