#pragma once

#include <vector>

#include "ecss/Types.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	// Ground built from a height function around a lake basin. The mesh is generated from these values when the
	// scene loads; local y = 0 is the water line, so a WaterComponent at the same height fills the basin.
	struct TerrainComponent {
		float size = 420.f;       // metres along each side
		int resolution = 321;     // vertices along each side
		// The lake's mean extent across (along x) and along (along z); its shore wanders in and out of that oval.
		float lakeWidth = 140.f;
		float lakeLength = 200.f;
		float lakeDepth = 7.f;
		float hillHeight = 16.f;
		int seed = 7;
		// A dirt road's course, x/z in the terrain's own space: a smooth curve through these points, levelled into
		// the ground, drawn as a track and kept clear of vegetation. Empty for none.
		std::vector<Math::Vec2> road;
		// Round clearings: x/z of the middle and the radius. Levelled to the height the middle had, trodden bare as
		// the road's sides are and kept clear of vegetation.
		std::vector<Math::Vec3> clearings;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::TerrainComponent> : std::true_type {};

using SFE::ComponentsModule::TerrainComponent;
