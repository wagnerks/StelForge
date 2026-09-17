#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "componentsModule/TerrainComponent.h"
#include "mathModule/Forward.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// The ground's growth (GrassComponent), drawn into the G-buffer after the opaque scene: clumps of grass, and reeds
	// and cattails in the shallows and lily pads further out.
	//
	// The grass is the forest kit's clumps: where they stand round the camera is worked out on the CPU a chunk at a
	// time, from a grass map baked once from the terrain's own rules and read back, and kept while the chunk is near.
	// Each frame the ones in view go up as one list, drawn a mesh and a level of detail at a time with the kit's
	// cut-out atlas, into the G-buffer and into the nearest shadow cascade, bending in the wind and away from the
	// character's feet.
	//
	// Reeds, cattails and pads keep nothing per plant: the camera's surroundings are cut into world-aligned chunks,
	// each chunk one instanced draw of a grid of them, and the vertex shader places, shapes, bends and colours every one
	// from its grid cell and the grass map.
	class GrassPass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		// The grass map read back once baked, the chunks near the camera placed, and the clumps in view uploaded.
		void prepare(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

		// Into a depth-only render pass of a directional light's cascade `layer`: the clumps near the camera.
		void drawShadow(Render::CommandListHandle cmd, const Math::Mat4& viewProj, uint32_t layer);

	private:
		// Mirrors GrassClumpDraw in grass_clump.vs.
		struct ClumpDrawParams {
			float viewProj[16]{};
			float params[4]{}; // x 1 into a light's view, y and z the distances the clumps shrink away between
		};
		// Mirrors SfeGrassClump in grass_clump.vs. std430.
		struct ClumpInstance {
			float posScale[4]{}; // xyz where it is rooted, w its scale
			float yawShade[4]{}; // x yaw in radians, y a brightness, z a phase for its sway, w its mesh
		};
		struct ClumpVertex {
			float position[3]{};
			float normal[3]{};
			float uv[2]{};
		};
		// One clump model in mClumpVertices and mClumpIndices: the full mesh and a coarser level.
		struct ClumpMesh {
			int32_t firstVertex = 0;
			std::array<uint32_t, 2> firstIndex{};
			std::array<uint32_t, 2> indexCount{};
		};
		static constexpr size_t kClumpLods = 2;

		void loadClumps();
		// The clumps of one chunk, placed the first time it is asked for; their mesh in yawShade.w.
		const std::vector<ClumpInstance>& chunkClumps(int cx, int cz);
		void drawClumps(Render::CommandListHandle cmd, PipelineHandle pipeline, const ClumpDrawParams& params, bool shadow);

		// Mirrors GrassDraw in grass.vs.
		struct DrawParams {
			float originX = 0.f;
			float originZ = 0.f;
			float spacing = 1.f;
			float side = 1.f;
			float width = 0.05f;
			float heightScale = 1.f;
			float fadeInStart = 0.f;
			float fadeInEnd = 0.f;
			float fadeOutStart = 0.f;
			float fadeOutEnd = 0.f;
			float tier = 0.f;
			float kind = 0.f; // 1 reed leaves, 2 lily pads, 3 cattails
		};

		// Mirrors GrassBlock in grass.vs, grass.fs and grass_map.fs. std140.
		struct GrassParams {
			float terrain[4]{}; // xyz the terrain's world position, w its size
			float grid[4]{};    // x heights per side, y metres between them, w min terrain height
			float trample[4]{}; // xyz the character's feet, w how far round them the grass is pushed aside
			float camera[4]{};  // xyz
			float litter[4]{};  // x how far under the water line and y how far over it the litter lies; z 1 when there is any
		};

		void bakeMap(Render::CommandListHandle cmd, const TransientAllocation& block);

		bool mEnabled = false;
		float mMinHeight = 0.f;
		float mMaxHeight = 0.f;
		// The terrain's lowest and highest ground per square of kBandCell metres, row by row along z: which chunks can
		// hold grass, reeds or lily pads at all.
		int mBandSide = 0;
		std::vector<float> mBandMin;
		std::vector<float> mBandMax;
		GrassParams mParams;

		TextureHandle mHeights{};
		TextureHandle mMap{};
		// VegetationPlacementsComponent::forestMap, or a black texel where nothing grows a forest.
		TextureHandle mForest{};
		TextureHandle mTerrainNoise{};
		uint32_t mMapSize = 0;
		FramebufferHandle mMapFbo{};
		bool mMapBaked = false;
		SamplerHandle mNearest{};
		SamplerHandle mLinear{};
		// Plants drawn as meshes, indexed triangles: reed clumps finest first, cattails finest first, lily pads finest
		// first, and a lily flower. For reeds every vertex's position is (along its part, side), its texCoords.x the part
		// (0.. a leaf, kReedStem, kReedHead, kReedSpike); for lilies see appendLilyPad and appendLilyFlower. grass.vs
		// shapes them.
		BufferHandle mPlants{};
		BufferHandle mPlantIndices{};
		struct PlantMesh {
			uint32_t firstIndex = 0;
			uint32_t indexCount = 0;
			int32_t firstVertex = 0;
		};
		std::array<PlantMesh, 9> mPlantMeshes{};

		RenderPassHandle mMapPass{};
		PipelineHandle mMapPipeline{};
		DescriptorSetLayoutHandle mMapLayout{};
		DescriptorSetHandle mMapSet{};

		RenderPassHandle mGBufferPass{};
		PipelineHandle mLilyPipeline{};
		PipelineHandle mPlantPipeline{}; // the plant meshes; lily pads and flowers have mLilyPipeline
		DescriptorSetLayoutHandle mLayout{};
		std::vector<DescriptorSetHandle> mSets; // one per frame slot

		// The clumps.
		ComponentsModule::TerrainComponent mTerrain;
		std::vector<uint8_t> mMapTexels; // the grass map read back: RGBA8, row by row
		int mMapTexelsSide = 0;
		uint32_t mFramesSinceBake = 0;
		std::vector<ClumpMesh> mClumpMeshes;
		BufferHandle mClumpVertices{};
		BufferHandle mClumpIndices{};
		TextureHandle mAtlas{};
		SamplerHandle mAtlasSampler{};
		std::unordered_map<int64_t, std::vector<ClumpInstance>> mChunks;
		// From the graphics settings: metres between the cells of a chunk's grid, the reach, and where they fade.
		float mClumpSpacing = 0.55f;
		float mClumpReach = 90.f;
		float mClumpFadeStart = 66.f;
		// The frame's clumps in view, by mesh and level: where each run starts in the uploaded list and how long it is.
		std::vector<std::vector<ClumpInstance>> mRuns;
		std::vector<uint32_t> mRunFirst;
		std::vector<uint32_t> mRunCount;
		std::vector<ClumpInstance> mVisible;
		std::vector<BufferHandle> mClumpBuffers; // per frame slot
		std::vector<size_t> mClumpCapacity;
		TransientAllocation mBlock{};
		bool mClumpsReady = false;
		RenderPassHandle mShadowPass{};
		PipelineHandle mClumpPipeline{};
		PipelineHandle mClumpShadowPipeline{};
		DescriptorSetLayoutHandle mClumpLayout{};
		std::vector<DescriptorSetHandle> mClumpSets; // per frame slot
	};
}
