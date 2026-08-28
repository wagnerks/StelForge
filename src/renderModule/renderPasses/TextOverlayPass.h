#pragma once

#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render {
	struct Font;
}

namespace SFE::Render::RenderPasses {

	class TextOverlayPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		RenderPassHandle mRenderPass;
		Font* mFont = nullptr;
		char mOverlayText[256]{};
	};

} // namespace SFE::Render::RenderPasses
