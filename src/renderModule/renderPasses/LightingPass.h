#pragma once
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class LightingPass : public RenderPass {
	public:
		LightingPass();
		void render(SystemsModule::RenderData& renderDataHandle) override;
	private:
		bool skyParams = false;
	};
}
