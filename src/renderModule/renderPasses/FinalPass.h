#pragma once

#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class FinalPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		FramebufferHandle mSrcFb;
	};
}
