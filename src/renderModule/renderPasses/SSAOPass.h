#pragma once
#include <vec3.hpp>
#include <vector>

#include "renderModule/RenderPass.h"

namespace GameEngine::RenderModule::RenderPasses {
	class SSAOPass : public RenderPass {
	public:
		struct Data {
			unsigned int ssaoColorBuffer = 0;
			unsigned int ssaoColorBufferBlur = 0;
		};

		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;

		std::vector<glm::vec3> ssaoKernel;
		unsigned int noiseTexture;
		unsigned int ssaoFBO, ssaoBlurFBO;
		unsigned int ssaoColorBuffer, ssaoColorBufferBlur;

		int kernelSize = 64;
		float radius = 0.5f;
		float bias = 0.01f;
	};
}
