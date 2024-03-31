#pragma once
#include "RenderPass.h"
#include "core/Engine.h"
#include "glWrapper/Framebuffer.h"

namespace SFE::Render::RenderPasses {
	class OcclusionPass : public RenderPass {
	public:
		~OcclusionPass() override;
		void init() override;
		void render(SystemsModule::RenderData& renderDataHandle) override;

		GLW::Framebuffer occlusionFrameBuffer;
		GLW::Texture depthTex;

		const float w = Engine::instance()->getWindow()->getScreenData().renderW / 16.f;
		const float h = Engine::instance()->getWindow()->getScreenData().renderH / 16.f;
	};
}
