#pragma once

#include "rhi/RenderHardwareInterface.h"

#include <utility>
#include <vector>

struct ImDrawData;

namespace SFE::Render {
	class ImGuiRHI {
	public:
		void init(RenderHardwareInterface* rhi);
		void shutdown();
		void createFontsTexture();
		void render(CommandListHandle cmd, ImDrawData* drawData, FramebufferHandle target, int width, int height);
		bool ready() const { return static_cast<bool>(mPipeline); }

	private:
		static constexpr int kFramesInFlight = 2;

		void ensureBuffers(size_t vtxBytes, size_t idxBytes);
		DescriptorSetHandle descriptorFor(TextureHandle texture);
		void bindDrawState(CommandListHandle cmd);

		RenderHardwareInterface* mRhi = nullptr;
		RenderPassHandle mRenderPass{};
		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		SamplerHandle mSampler{};
		TextureHandle mFontTex{};
		DescriptorSetHandle mFontSet{};
		BufferHandle mVb[kFramesInFlight]{};
		BufferHandle mIb[kFramesInFlight]{};
		size_t mVbCap[kFramesInFlight]{};
		size_t mIbCap[kFramesInFlight]{};
		int mWriteFrame = 0;
		std::vector<std::pair<uint32_t, DescriptorSetHandle>> mExtraSets;
	};
}
