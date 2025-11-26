#pragma once
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"
#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void render(SystemsModule::RenderData& renderDataHandle) override;

		BufferHandle trianglesVBO;
		BufferHandle linesVBO;

		PipelineHandle trianglesPipeline;
		PipelineHandle linesPipeline;
	};
}
