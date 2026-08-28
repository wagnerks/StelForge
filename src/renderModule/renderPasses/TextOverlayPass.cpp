#include "TextOverlayPass.h"

#include "core/Engine.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/TextRenderer.h"
#include "systemsModule/systems/RenderSystem.h"

#include <cstdio>

namespace SFE::Render::RenderPasses {

	void TextOverlayPass::init(SystemsModule::RenderData&)
	{
		TextRenderer::instance();
		mFont = FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 22);

		RenderPassDesc rpd;
		rpd.colorAttachmentCount = 1;
		rpd.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		rpd.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		mRenderPass = Engine::instance()->rhi()->createRenderPass(rpd);
	}

	void TextOverlayPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd)
	{
		if (!mFont || !mRenderPass) {
			return;
		}

		const auto* info = SystemsModule::RenderDebugInfo::instance();
		std::snprintf(
			mOverlayText,
			sizeof(mOverlayText),
			"FPS: %d\nGPU: %.1f ms  CPU: %.1f ms",
			Engine::instance()->getFPS(),
			static_cast<double>(info->gpuMs),
			static_cast<double>(info->cpuMs)
		);

		auto* rhi = Engine::instance()->rhi();
		const auto fb = renderDataHandle.frame.framebuffer(FrameId::Final);
		const auto& screen = Engine::instance()->getWindow()->getScreenData();

		RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.renderPass = mRenderPass;
		rpBeginInfo.framebuffer = fb;
		rpBeginInfo.width = screen.renderW;
		rpBeginInfo.height = screen.renderH;
		rhi->cmdBeginRenderPass(cmd, rpBeginInfo);

		TextRenderer::instance()->renderText(
			cmd,
			mOverlayText,
			14.f,
			34.f,
			1.f,
			Math::Vec3{ 0.95f, 0.98f, 1.f },
			mFont
		);

		rhi->cmdEndRenderPass(cmd);
		if (fb && Engine::instance()->presentToScreen()) {
			rhi->cmdBlit(cmd, fb, rhi->getBackbufferFramebuffer(), BlitMask::Color, BlitFilter::Nearest);
		}
	}

} // namespace SFE::Render::RenderPasses
