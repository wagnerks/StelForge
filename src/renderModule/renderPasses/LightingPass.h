#pragma once
#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class LightingPass : public RenderPass {
	public:
		LightingPass();
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		bool skyParams = false;
	};
}
