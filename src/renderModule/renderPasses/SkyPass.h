#pragma once

#include "renderModule/renderPasses/FinalDepthTargets.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// The atmosphere behind everything, for a scene with a SkyComponent: drawn on the far plane after lighting, so
	// only the pixels no surface covered take it.
	class SkyPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		FinalDepthTargets mTargets;
		RenderPassHandle mRenderPass{};
		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		std::vector<DescriptorSetHandle> mSets; // one per frame slot
	};
}
