#pragma once
#include "glWrapper/Buffer.h"
#include "glWrapper/VertexArray.h"
#include "renderModule/Utils.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void render(SystemsModule::RenderData& renderDataHandle) override;
		GLW::VertexArray trianglesVAO;
		GLW::Buffer<GLW::ARRAY_BUFFER, Utils::Triangle> trianglesVBO;

		GLW::VertexArray linesVAO;
		GLW::Buffer<GLW::ARRAY_BUFFER, Math::Vec3> linesVBO;
	};
}
