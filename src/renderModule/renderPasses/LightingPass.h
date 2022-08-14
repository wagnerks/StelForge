#pragma once
#include "renderModule/RenderPass.h"

namespace GameEngine::RenderModule::RenderPasses {
	class LightingPass : public RenderPass {
	public:
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	};
}
