#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "assetsModule/modelModule/BoundingVolume.h"
#include "mathModule/Forward.h"
#include "renderModule/MaterialTextures.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// Far vegetation as pictures of itself. Every model of a VegetationLayer with `impostor` is baked once, from
	// kViews directions around it and at a few sizes, into an albedo and a normal atlas; then each placement with no
	// entity of its own (VegetationSystem) is one upright quad turned to the viewer, showing the views nearest to
	// where it is seen from at the size it covers. Drawn into the G-buffer after the grass, and by the shadow pass
	// into its cascades.
	class ImpostorPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

		// Outside a render pass, before the first draw of a frame: bakes the atlas the first time.
		void bakeIfNeeded(Render::CommandListHandle cmd);
		// Into a render pass with the G-buffer's attachments. `cameraUbo` holds the view's SharedMatrices, `texelsPerMetre`
		// how many pixels a metre covers a metre away.
		void drawGBuffer(Render::CommandListHandle cmd, BufferHandle cameraUbo, const FrustumModule::Frustum& frustum, const Math::Vec3& eye,
			float texelsPerMetre);
		// Into a depth-only render pass of a directional light. `direction` is the way its light travels.
		void drawShadow(Render::CommandListHandle cmd, BufferHandle cameraUbo, const Math::Mat4& viewProj, const FrustumModule::Frustum& frustum,
			const Math::Vec3& direction, float texelsPerMetre);

	private:
		// Mirrors SfeImpostorInstance in sfe_impostor.glsl.
		struct GpuInstance {
			float posScale[4]{};
			float yawModel[4]{};
		};
		// Mirrors ImpostorDraw in sfe_impostor.glsl.
		struct DrawParams {
			float viewProj[16]{};
			float eye[4]{};
			float atlas[4]{};
			float params[4]{};
		};
		// Mirrors ImpostorBake in impostor_bake.vs.
		struct BakeParams {
			float view[4]{};
			float frame[4]{};
			float tint[4]{};
		};
		struct Cell {
			uint32_t first = 0;
			uint32_t count = 0;
			float minY = 0.f;
			float maxY = 0.f;
		};

		void bake(Render::CommandListHandle cmd);
		void releaseBakeTarget();
		DescriptorSetHandle setFor(BufferHandle cameraUbo);
		void drawCells(Render::CommandListHandle cmd, PipelineHandle pipeline, const DrawParams& params, BufferHandle cameraUbo,
			const FrustumModule::Frustum& frustum);

		bool mEnabled = false;
		bool mBaked = false;
		uint32_t mFramesSinceBake = 0;
		std::vector<std::string> mModels;
		std::vector<float> mModelFrames; // 4 per model, as the GPU reads them
		float mMaxHalfSize = 0.f;

		float mCellSize = 0.f;
		int mCellsPerSide = 0;
		Math::Vec2 mOrigin;
		std::vector<Cell> mCells;
		size_t mPlacements = 0;

		uint32_t mAtlasWidth = 0;
		uint32_t mAtlasHeight = 0;
		TextureHandle mAlbedo{};
		TextureHandle mNormal{};
		TextureHandle mBakeDepth{};
		FramebufferHandle mBakeFbo{};
		RenderPassHandle mBakePass{};
		PipelineHandle mBakePipeline{};
		DescriptorSetLayoutHandle mBakeLayout{};
		SingleTextureSets mBakeTextures;
		SamplerHandle mSampler{};

		BufferHandle mQuad{};
		BufferHandle mInstances{};
		BufferHandle mModelsBuffer{};
		// Per frame slot: which placements have an entity, as last uploaded there.
		std::vector<BufferHandle> mBits;
		std::vector<uint64_t> mBitsUploaded;
		std::vector<uint32_t> mBitsData;
		uint64_t mBitsVersion = 0;

		RenderPassHandle mGBufferPass{};
		RenderPassHandle mShadowPass{};
		PipelineHandle mPipeline{};
		PipelineHandle mShadowPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		// Per frame slot, per camera buffer.
		std::vector<std::unordered_map<uint32_t, DescriptorSetHandle>> mSets;
	};
}
