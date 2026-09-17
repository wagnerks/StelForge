#pragma once

#include <string>
#include <vector>

#include "componentsModule/ComponentBase.h"
#include "componentsModule/WindComponent.h"
#include "mathModule/Forward.h"
#include "rhi/RenderHardwareInterface.h"

namespace SFE::ComponentsModule {
	// Metres a texel of the forest map covers; sfe_terrain.glsl has the same.
	inline constexpr float kForestTexelMetres = 2.f;

	// A texture laid over part of a layer's models, bark on trunks or stone on rocks: on the parts drawn with the
	// texture `on` whose texture coordinates fall in `uv` (u min, v min, u max, v max), `texture`_albedo.png and
	// _normal.png, a repeat every `metres`.
	struct VegetationSurface {
		std::string on;
		std::string texture;
		std::vector<float> uv;
		float metres = 1.f;
		bool rotate = false; // the texture's u and v swapped, for grain along a log lying down
	};

	// One kind of growth scattered over a terrain: which models, how many, and where they may stand.
	struct VegetationLayer {
		std::vector<std::string> models;
		// Beside `models`, one each: a model spawned with the same transform as that one, such as the roots at a tree's
		// foot; empty for none.
		std::vector<std::string> rootModels;
		int count = 100;            // placements tried; the rules below turn some of them down
		float minScale = 1.f;
		float maxScale = 1.f;
		float minHeight = 0.5f;     // metres above the water line
		float maxHeight = 1000.f;
		float maxSlope = 35.f;      // degrees
		float spacing = 0.f;        // metres kept between any two of this layer
		float noiseScale = 0.f;     // groves and clearings: the frequency of a density noise, 0 for none
		float noiseThreshold = 0.5f; // grows only where that noise is above this
		float sink = 0.f;           // metres pushed into the ground, so a trunk on a slope has no gap under it
		float colliderRadius = 0.f; // a capsule the character cannot walk through, at scale 1; 0 for none
		float colliderHeight = 0.f;
		// Degrees tipped over around a horizontal axis: 90 lays a tree down as a fallen log. Tipped layers get no
		// upright collider and no impostor.
		float pitch = 0.f;
		// Metres from the camera within which the models are real entities; 0 for everywhere. Past it the layer
		// is not drawn, or drawn as a picture of itself when `impostor` is set.
		float drawDistance = 0.f;
		bool impostor = false;
		Sway sway = Sway::None;
		// Metres round each of this layer's trees, at scale 1, that count as forest: the forest map is made of these.
		float canopy = 0.f;
		// Grows only where the forest map is within these: 0 open ground, 1 deep in the forest. Layers with a canopy
		// make the map and ignore them.
		float forestMin = 0.f;
		float forestMax = 1.f;
		// Up to MaterialComponent::kTriplanarLayers per material; none for the models' own look.
		std::vector<VegetationSurface> surfaces;
	};

	// Growth scattered over the TerrainComponent on the same entity when the scene loads. The same seed gives the
	// same forest.
	struct VegetationComponent {
		int seed = 1;
		std::vector<VegetationLayer> layers;
	};

	// One placement of a VegetationLayer: a model standing at `pos`, tipped `pitch` degrees over and turned `yaw` degrees
	// around the vertical.
	struct VegetationInstance {
		Math::Vec3 pos;
		float scale = 1.f;
		float yaw = 0.f;
		float pitch = 0.f;
		uint16_t layer = 0;
		uint16_t model = 0; // into the layer's models
	};

	// Everything a VegetationComponent grows, generated from it when the scene loads. The instances are sorted by
	// square cells of `cellSize` metres, row by row along z, so any cell's are one contiguous range.
	struct VegetationPlacementsComponent {
		float cellSize = 32.f;
		int cellsPerSide = 0;
		Math::Vec2 origin;               // world x/z of the first cell's corner
		std::vector<uint32_t> cellStart; // cellsPerSide squared plus one: cell i is [cellStart[i], cellStart[i + 1])
		std::vector<VegetationInstance> instances;
		// How deep in the forest each spot of the terrain is, 0..1, R8, kForestTexelMetres a texel, centred on the
		// terrain. The ground and the grass change with it.
		Render::TextureHandle forestMap;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::VegetationComponent> : std::true_type {};
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::VegetationPlacementsComponent> : std::true_type {};

using SFE::ComponentsModule::VegetationComponent;
using SFE::ComponentsModule::VegetationPlacementsComponent;
