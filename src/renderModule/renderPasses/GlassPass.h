#pragma once
#include "renderModule/renderPasses/RenderPass.h"
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render::RenderPasses {
	class GlassPass : public RenderPassWithData {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void prepare() override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		struct Params {
			float viewPosX = 0.f;
			float viewPosY = 0.f;
			float viewPosZ = 0.f;
			float _pad2 = 0.f;
			float resolutionX = 1.f;
			float resolutionY = 1.f;
			float _pad3 = 0.f;
			float roughness = 0.05f;
			float lightX = 0.f;
			float lightY = -1.f;
			float lightZ = 0.f;
			float _pad0 = 0.f;
			float colorX = 1.f;
			float colorY = 1.f;
			float colorZ = 1.f;
			float _pad1 = 0.f;
			float _pad4[3]{};
			float sphereSpacing = 8.f;
		};

		void ensureTargets(SystemsModule::RenderData& renderDataHandle);

		Params mParams;
		BufferHandle mParamsUBO{};
		TextureHandle mSceneCopy{};
		SamplerHandle mSampler{};
		FramebufferHandle mSceneCopyFbo{};
		FramebufferHandle mGlassFbo{};
		TextureHandle mFinalTex{};
		TextureHandle mDepthTex{};
		PipelineHandle mPipeline{};
		RenderPassHandle mRenderPass{};
		DescriptorSetLayoutHandle mLayout{};
		DescriptorSetHandle mSet{};
	};
}
