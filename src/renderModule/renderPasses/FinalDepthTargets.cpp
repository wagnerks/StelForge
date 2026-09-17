#include "FinalDepthTargets.h"

#include "core/Engine.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render::RenderPasses {
	FramebufferHandle FinalDepthTargets::get(const SystemsModule::RenderData& renderData, const RenderPassHandle renderPass) {
		const auto color = renderData.frame.texture(FrameId::Final);
		const auto depth = renderData.frame.texture(FrameId::GBufferDepth);
		if (!color || !depth) {
			return {};
		}
		for (const auto& entry : mEntries) {
			if (entry.color.id == color.id && entry.depth.id == depth.id) {
				return entry.fbo;
			}
		}

		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		FramebufferDesc desc;
		desc.width = screen.renderW;
		desc.height = screen.renderH;
		desc.renderPass = renderPass;
		desc.colorAttachments = { { color } };
		desc.depthStencilAttachment = { .texture = depth };
		const auto fbo = Engine::instance()->rhi()->createFramebuffer(desc);
		mEntries.push_back({ color, depth, fbo });
		return fbo;
	}

	RenderPassHandle createFinalDepthRenderPass() {
		RenderPassDesc desc;
		desc.colorAttachmentCount = 1;
		desc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		desc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		desc.colorAttachments[0].format = TextureFormat::RGBA16F;
		desc.hasDepthStencil = true;
		desc.depthAttachment.loadOp = AttachmentLoadOp::Load;
		desc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		desc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT24;
		return Engine::instance()->rhi()->createRenderPass(desc);
	}
}
