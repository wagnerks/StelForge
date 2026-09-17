#pragma once

#include "renderModule/renderPasses/FinalDepthTargets.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// Water surfaces (WaterComponent) in a scene with a sky. The surface reflects what the screen already shows by
	// marching its reflected rays through the scene's depth, and the sky where they find nothing; under it the lit
	// image, refracted and dimmed by how much water the light crosses.
	class WaterPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		SamplerHandle mNearest{};
		SamplerHandle mLinear{};
		// The ripple map: repeating and trilinear, with no anisotropy. At a grazing angle an anisotropic read of it
		// costs more than every wave the shader sums.
		SamplerHandle mRipple{};

		TextureHandle mSceneCopy{};
		FramebufferHandle mSceneCopyFbo{};
		FinalDepthTargets mTargets;
		RenderPassHandle mSurfacePass{};
		PipelineHandle mSurfacePipeline{};
		DescriptorSetLayoutHandle mSurfaceLayout{};
		std::vector<DescriptorSetHandle> mSurfaceSets; // one per frame slot
	};
}
