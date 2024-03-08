#pragma once
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void render(SystemsModule::RenderData& renderDataHandle) override;
		GLW::VertexArray trianglesVAO;
		GLW::Buffer trianglesVBO{GLW::ARRAY_BUFFER};

		GLW::VertexArray linesVAO;
		GLW::Buffer linesVBO{GLW::ARRAY_BUFFER};
	};
}
