#pragma once
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render {
	struct Font;
}

namespace SFE::Render::RenderPasses {
	class FinalPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		PipelineHandle makeFullscreen(RenderHardwareInterface* rhi, RenderPassHandle renderPass, const char* vsPath, const char* fsPath);

		PipelineHandle mBloomPipeline{};
		PipelineHandle mPostPipeline{};
		RenderPassHandle mBloomPass{};
		RenderPassHandle mDisplayPass{};
		DescriptorSetLayoutHandle mLayout{};
		DescriptorSetHandle mBloomSet{};
		DescriptorSetHandle mPostSet{};
		SamplerHandle mSampler{};
		BufferHandle mPostUBO{};

		struct PostParams {
			float bloomStrength = 0.62f;
			float exposure = 0.92f;
			float vignette = 0.14f;
			float ca = 0.0028f;
		} mPostParams;

		TextureHandle mBloomTex{};
		FramebufferHandle mBloomFbo{};
		TextureHandle mDisplayTex{};
		FramebufferHandle mDisplayFbo{};
		Font* mOverlayFont = nullptr;
	};
}
