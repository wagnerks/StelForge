#pragma once

namespace SFE::ComponentsModule {
	// A still body of water: a level square surface centred on the entity's position, at its height. Drawn by
	// WaterPass with the scene reflected in it and the ground under it seen through, for a scene with a sky.
	struct WaterComponent {
		float size = 300.f;  // metres along each side
		float clarity = 4.f; // metres through which the bottom is still clearly seen
	};
}

using SFE::ComponentsModule::WaterComponent;
