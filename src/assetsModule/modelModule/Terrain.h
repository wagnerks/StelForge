#pragma once

#include <cstdint>
#include <vector>

#include "componentsModule/TerrainComponent.h"
#include "rhi/RenderHardwareInterface.h"

namespace AssetsModule {
	class Model;

	// The terrain mesh for these parameters, generated once per distinct set and kept as an asset.
	// texCoords carry world x/z in metres, texCoords1.x the height above the water line: the terrain shader
	// shades from those instead of textures.
	// Cells a side of one of the terrain mesh's chunks.
	inline constexpr int kTerrainChunkCells = 32;
	Model* getOrCreateTerrain(const SFE::ComponentsModule::TerrainComponent& terrain);

	// The heights the terrain mesh is built from, row by row along z: resolution x resolution samples spaced
	// size / (resolution - 1) apart, from -size/2 to +size/2 on both axes, in the terrain's local space.
	// Computed once per distinct terrain and kept, on disk too.
	const std::vector<float>& terrainHeights(const SFE::ComponentsModule::TerrainComponent& terrain);
	// A hash of everything the terrain is built from, for caches of what is built on top of it.
	uint64_t terrainCacheKey(const SFE::ComponentsModule::TerrainComponent& terrain);

	// The height of the terrain mesh at x/z in its local space, on the very triangle drawn there, so whatever is
	// stood on it neither floats nor sinks. Its gradient comes back through `slopeX` and `slopeZ` when asked for.
	// `heights` are terrainHeights(terrain).
	float terrainSurfaceHeight(const SFE::ComponentsModule::TerrainComponent& terrain, const std::vector<float>& heights, float x, float z,
		float* slopeX = nullptr, float* slopeZ = nullptr);

	// Metres a texel of terrainNoiseTexture covers; sfe_terrain.glsl has the same.
	constexpr float kTerrainNoiseTexelMetres = 4.f;
	// The terrain shader's slow noise, sfeFbm exactly, baked once per terrain instead of for every pixel: r the broad
	// variation, g where sandy coves are, b which of a material's two tilings shows, a the signed distance to the road,
	// terrainRoadDistance / kRoadTextureRange + 0.5. Centred on the terrain, a texel every kTerrainNoiseTexelMetres.
	// The terrain's material and the grass map read it.
	constexpr float kRoadTextureRange = 16.f;
	SFE::Render::TextureHandle terrainNoiseTexture(const SFE::ComponentsModule::TerrainComponent& terrain);

	// Metres from the terrain's road's centreline, x/z in the terrain's own space, signed by the side of it; far from
	// any road (or with none) kRoadFar. The road itself is kRoadHalfWidth either side of the line.
	constexpr float kRoadFar = 1000.f;
	constexpr float kRoadHalfWidth = 1.9f;
	float terrainRoadDistance(const SFE::ComponentsModule::TerrainComponent& terrain, float x, float z);

	// Smooth noise in [0, 1], the one the terrain is shaped with; for anything laid out over it.
	float terrainNoise(float x, float z, int seed, int octaves);

	constexpr const char* kTerrainFS = "shaders/g_buffer_terrain.fs";
}
