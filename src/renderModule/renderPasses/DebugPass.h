#pragma once
#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
		unsigned linesVAO;
		unsigned cubeVBO;
	};
}
