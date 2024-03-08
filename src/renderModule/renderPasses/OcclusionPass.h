#pragma once
#include "RenderPass.h"
#include "glWrapper/Framebuffer.h"
#include "renderModule/Renderer.h"

namespace SFE::Render::RenderPasses {
	class OcclusionPass : public RenderPass {
	public:
		~OcclusionPass() override;
		void init() override;
		void render(SystemsModule::RenderData& renderDataHandle) override;

		GLW::Framebuffer occlusionFrameBuffer;
		GLW::Texture depthTex;

		const float w = Renderer::screenDrawData.renderW / 32.f;
		const float h = Renderer::screenDrawData.renderH / 32.f;
	};
}
