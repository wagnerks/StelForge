#pragma once

#include <vector>

#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class SSAOPass : public RenderPass {
	public:
		struct Data {
			std::vector<Math::Vec3> mSsaoKernel;

			TextureHandle mNoiseTexture;
			FramebufferHandle mSsaoFbo;
			FramebufferHandle mSsaoBlurFbo;
			TextureHandle mSsaoColorBuffer;
			TextureHandle mSsaoColorBufferBlur;

			SamplerHandle sampler;

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
		
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		struct SSAOParams
		{
			float samples = 12.f;
			float intensity = 1.0f;
			float scale = 0.5f;
			float bias = 0.025f;
			float sample_rad = 0.45f;
			float max_distance = 2.4f;
			// TAA jitter phase. Rotates the sample disc per frame so the static screen-space
			// pattern averages out in the TAA resolve instead of standing still as ribbing.
			float frameIndex = 0.f;
		};

		SSAOParams params;
		BufferHandle ssaoParamsBuffer;

		struct SSAOBlurParams
		{
			float sigmaS = 1.f;
			float sigmaL = 1.f;
			float facS = 1.f;
			float facL = 1.f;

			void updateFac()
			{
				facL = -1.f / (2.f * sigmaL * sigmaL);
				facS = -1.f / (2.f * sigmaS * sigmaS);
			}
		};

		SSAOBlurParams blurParams;
		BufferHandle ssaoBlurParamsBuffer;

		bool ssaoDebugWindow = false;
		Data mData{};

		PipelineHandle mainPipeline;
		PipelineHandle blurPipeline;

		RenderPassHandle main;
		RenderPassHandle blur;

		Render::DescriptorSetLayoutHandle mainTexSetLayout;
		Render::DescriptorSetHandle       mainTexSet;

		Render::DescriptorSetLayoutHandle blurTexSetLayout;
		Render::DescriptorSetHandle       blurTexSet;

		int mSsaoW = 0;
		int mSsaoH = 0;
	};
}
