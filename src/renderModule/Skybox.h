#pragma once

#include "assetsModule/shaderModule/Shader.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"

namespace SFE::Render {
	class Skybox {
	public:
		Skybox(std::string_view path);
		~Skybox();
		void init();
		void draw();
	private:
		ShaderModule::ShaderBase* skyboxShader = nullptr;
		GLW::VertexArray VAO;
		GLW::Buffer VBO;
		unsigned cubemapTex = -1;
		std::string skyboxPath;
	};
}
