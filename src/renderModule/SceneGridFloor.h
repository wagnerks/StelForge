#pragma once


#include "RenderHardwareInterface.h"
#include "assetsModule/shaderModule/ShaderBase.h"

namespace SFE::Render {
	class SceneGridFloor {
	public:
		SceneGridFloor();
		~SceneGridFloor();
		
		void draw();
	private:
		void init();

		PipelineHandle floorPipeline;
		BufferHandle VBO;

		Math::Mat4 transform = Math::Mat4(1.f);
	};
}

