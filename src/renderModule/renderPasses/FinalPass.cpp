#include "FinalPass.h"

#include "core/Engine.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

void FinalPass::init(SystemsModule::RenderData& renderDataHandle) {
	if (!renderDataHandle.finalTexture) {
		return;
	}

	auto* window = Engine::instance()->getWindow();
	FramebufferDesc fbDesc;
	fbDesc.width = window ? static_cast<uint32_t>(window->getScreenData().renderW) : 0;
	fbDesc.height = window ? static_cast<uint32_t>(window->getScreenData().renderH) : 0;
	fbDesc.colorAttachments.push_back({ renderDataHandle.finalTexture });
	mSrcFb = Engine::instance()->rhi()->createFramebuffer(fbDesc);
}

void FinalPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!renderDataHandle.finalTexture) {
		return;
	}

	auto* rhi = Engine::instance()->rhi();
	if (!mSrcFb) {
		init(renderDataHandle);
	}
	if (!mSrcFb) {
		return;
	}

	rhi->cmdBlit(cmd, mSrcFb, {}, BlitMask::Color, BlitFilter::Nearest);
}
