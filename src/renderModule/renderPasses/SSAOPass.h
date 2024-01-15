#pragma once

#include <vector>

#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class SSAOPass : public RenderPass {
	public:
		struct Data {
			std::vector<Math::Vec3> mSsaoKernel;
			unsigned int mNoiseTexture = 0;
			unsigned int mSsaoFbo = 0;
			unsigned int mSsaoBlurFbo = 0;
			unsigned int mSsaoColorBuffer = 0;
			unsigned int mSsaoColorBufferBlur = 0;
			int mKernelSize = 16;
			float mRadius = 0.5f;
			float mBias = 0.7f;
			int samples = 16;
			float intencity = 1.5f;
			float scale = 0.5f;
			float sample_rad = 1.2f;
			float max_distance = 0.01f;

			float sigmaS = 1.0f;
			float sigmaL = 1.2f;
		};

		void init() override;
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:

		Data mData{};
	};
}
