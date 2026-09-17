#pragma once

#include <vector>

#include "assetsModule/RenderMeshData.h"
#include "mathModule/Forward.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// GarbageSystem's litter where no entity stands in for it: every piece in one buffer, drawn a kind and a level of
	// detail at a time into the G-buffer with the default surface shader, and into the nearest shadow cascade. The
	// camera's cells are culled on the CPU, and a piece too small on screen is left out; what is left goes up as one
	// list of piece indices per frame.
	class GarbagePass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
		// Uploads what moved and the frame's visible pieces, once for all the draws that use them.
		void prepare(SystemsModule::RenderData& renderDataHandle) override;

		// Into a depth-only render pass of a directional light's cascade `layer`.
		void drawShadow(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd, const Math::Mat4& viewProj, uint32_t layer);

	private:
		// Mirrors GarbageDraw in garbage.vs.
		struct DrawParams {
			float viewProj[16]{};
			float params[4]{}; // x 1 into a light's view, y the material
		};
		struct Submesh {
			std::vector<RenderMeshData> lods; // finest first
			uint32_t material = 0;
			bool cutout = false;
			DescriptorSetHandle textures;
		};
		struct Kind {
			std::vector<Submesh> submeshes;
		};
		static constexpr uint32_t kLods = 4;

		// GarbageSystem is made after the render passes: the pass sets itself up the first frame it finds it.
		bool setUp();
		void draw(Render::CommandListHandle cmd, bool shadow, const DrawParams& params);

		bool mTried = false;
		bool mEnabled = false;
		bool mPrepared = false;
		std::vector<Kind> mKinds;

		BufferHandle mPieces{};
		std::vector<BufferHandle> mVisible; // per frame slot
		std::vector<uint32_t> mVisibleData;
		// Per kind and level: where its run of mVisibleData starts and how long it is.
		std::vector<uint32_t> mRunFirst;
		std::vector<uint32_t> mRunCount;
		std::vector<std::vector<uint32_t>> mRuns; // scratch, per kind and level
		std::vector<uint32_t> mHidden;
		std::vector<uint32_t> mChanged;

		RenderPassHandle mGBufferPass{};
		RenderPassHandle mShadowPass{};
		PipelineHandle mSolidPipeline{};
		PipelineHandle mCutoutPipeline{};
		PipelineHandle mShadowPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		DescriptorSetLayoutHandle mTextureLayout{};
		std::vector<DescriptorSetHandle> mSets; // per frame slot
	};
}
