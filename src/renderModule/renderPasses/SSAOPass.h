#pragma once

#include <vector>

#include "glWrapper/Framebuffer.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class SSAOPass : public RenderPass {
	public:
		struct Data {
			std::vector<Math::Vec3> mSsaoKernel;
			GLW::Texture mNoiseTexture{GLW::TEXTURE_2D};
			GLW::Framebuffer mSsaoFbo;
			GLW::Framebuffer mSsaoBlurFbo;
			GLW::Texture mSsaoColorBuffer{GLW::TEXTURE_2D};
			GLW::Texture mSsaoColorBufferBlur{GLW::TEXTURE_2D};
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
		void render(SystemsModule::RenderData& renderDataHandle) override;
	private:

		bool ssaoDebugWindow = false;
		Data mData{};
	};
}
