#pragma once

namespace SFE::ComponentsModule {
	// A meadow of individual blades over the TerrainComponent on the same entity, drawn around the camera by
	// GrassPass wherever the terrain is painted as grass.
	struct GrassComponent {
		float density = 45.f; // blades per square metre next to the camera
		float height = 0.45f; // metres, before each blade's own variation
		float radius = 70.f;  // metres around the camera; it thins out towards this distance
	};
}

using SFE::ComponentsModule::GrassComponent;
