#pragma once

#include "RenderHardwareInterface.h"
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

		PipelineHandle skyboxPipeline;

		BufferHandle skyboxVBO;

		TextureHandle cubemapTex;
		std::string skyboxPath;
	};
}
