#pragma once
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class SheenPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		struct Params {
			float lightDirX = 0.f, lightDirY = -1.f, lightDirZ = 0.f;
			float sheenStrength = 1.f;
			float lightColorX = 1.f, lightColorY = 0.96f, lightColorZ = 0.88f;
			float rimStrength = 0.7f;
			float viewX = 0.f, viewY = 0.f, viewZ = 0.f;
			float wrapStrength = 1.f;
		};

		Params mParams;
		TextureHandle mColor{};
		FramebufferHandle mFbo{};
		SamplerHandle mLinear{};
		SamplerHandle mNearest{};
		PipelineHandle mPipeline{};
		RenderPassHandle mRenderPass{};
		DescriptorSetLayoutHandle mLayout{};
		DescriptorSetHandle mSet{};
	};
}
