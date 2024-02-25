#pragma once
#include "Buffer.h"
#include "VertexArray.h"
#include "assetsModule/shaderModule/Shader.h"

namespace SFE::Render {
	class Skybox {
	public:
		Skybox(std::string_view path);
		~Skybox();
		void init();
		void draw();
	private:
		ShaderModule::ShaderBase* skyboxShader = nullptr;
		VertexArray VAO;
		Buffer VBO;
		unsigned cubemapTex = -1;
		std::string skyboxPath;
	};
}
