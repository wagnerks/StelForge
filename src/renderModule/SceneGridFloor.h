#pragma once

#include "Buffer.h"
#include "VertexArray.h"
#include "assetsModule/shaderModule/ShaderBase.h"

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
		VertexArray VAO;
		Buffer VBO{ARRAY_BUFFER};
		Math::Mat4 transform = Math::Mat4(1.f);
	};
}

