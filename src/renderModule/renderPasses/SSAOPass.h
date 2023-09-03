#pragma once
#include <vec3.hpp>
#include <vector>

#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class SSAOPass : public RenderPass {
	public:
		struct Data {
			std::vector<glm::vec3> mSsaoKernel;
			unsigned int mNoiseTexture;
			unsigned int mSsaoFbo;
			unsigned int mSsaoBlurFbo = 0;
			unsigned int mSsaoColorBuffer = 0;
			unsigned int mSsaoColorBufferBlur = 0;
			int mKernelSize = 10;
			float mRadius = 1.0f;
			float mBias = 0.7f;
			int samples = 16;
			float intencity = 1.f;
			float scale = 2.5f;
			float sample_rad = 0.2f;
			float max_distance = 0.7f;

			float sigmaS = 0.5f;
			float sigmaL = 0.5f;
		};

		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		
		Data mData{};
	};
}
