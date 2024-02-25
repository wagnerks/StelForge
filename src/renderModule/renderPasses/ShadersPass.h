#pragma once
#include "renderModule/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class ShadersPass : public RenderPass {
	public:
		ShadersPass();
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	};
}
