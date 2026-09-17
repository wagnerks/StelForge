#pragma once

#include <vector>

#include "renderModule/renderPasses/FinalDepthTargets.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// Mist lying on the water and a thin haze in the distance, for a scene with a SkyComponent, and the fade into that
	// haze where the view distance ends.
	// Both are fog of an exact thickness along each view ray -- the mist thinning out exponentially with height above
	// the water line, drifting in patches with the wind -- lit by the sky and the sun, and blended over the finished
	// image after the water.
	class MistPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		FinalDepthTargets mTargets;
		RenderPassHandle mRenderPass{};
		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		SamplerHandle mNearest{};
		std::vector<DescriptorSetHandle> mSets; // one per frame slot
	};
}
