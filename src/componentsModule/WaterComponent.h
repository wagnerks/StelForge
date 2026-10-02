#pragma once

#include "componentsModule/WindComponent.h"

namespace SFE::ComponentsModule {
	// A still body of water: a level square surface centred on the entity's position, at its height. Drawn by
	// WaterPass with the scene reflected in it and the ground under it seen through, for a scene with a sky.
	struct WaterComponent {
		float size = 300.f;  // metres along each side
		float clarity = 4.f; // metres through which the bottom is still clearly seen
		float drift = 0.3f;  // metres a second the wind drags the surface along at full strength (a few % of the wind)

		// The surface current `wind` drives, on the ground (x, z), metres a second: what floats goes with it.
		Math::Vec2 current(const WindComponent& wind) const {
			const auto way = wind.blowing();
			return { way.x * wind.strength * drift, way.y * wind.strength * drift };
		}
	};
}

using SFE::ComponentsModule::WaterComponent;
