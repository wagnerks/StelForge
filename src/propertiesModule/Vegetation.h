#pragma once

#include "ecss/Types.h"

namespace SFE::PropertiesModule {
	// Scatters the VegetationComponent of `terrain` over its TerrainComponent: every layer's placements that pass
	// the height, slope, noise and spacing rules, standing on the terrain mesh, into a VegetationPlacementsComponent
	// on the same entity. VegetationSystem turns the ones near the camera into model entities.
	void generateVegetation(ecss::EntityId terrain);
}
