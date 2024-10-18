#pragma once


#include "assetsModule/shaderModule/ShaderBase.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"

namespace SFE::Render {
	class SceneGridFloor {
	public:
		SceneGridFloor();
		~SceneGridFloor();
		
		void draw();
	private:
		void init();
		SFE::ShaderModule::ShaderBase* floorShader = nullptr;
		size_t floorShaderHash;
		GLW::VertexArray VAO;
		GLW::Buffer<GLW::ARRAY_BUFFER, Math::Vec3> VBO;
		Math::Mat4 transform = Math::Mat4(1.f);
	};
}

