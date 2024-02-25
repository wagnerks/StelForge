#pragma once
#include "renderModule/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
		VertexArray linesVAO;
		Buffer cubeVBO{ARRAY_BUFFER};
	};
}
