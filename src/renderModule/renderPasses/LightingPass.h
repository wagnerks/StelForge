#pragma once
#include "renderModule/RenderPass.h"

namespace GameEngine::RenderModule::RenderPasses {
	class LightingPass : public RenderPass {
	public:
		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	};
}
