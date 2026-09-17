#pragma once

#include <string>
#include <vector>

#include "ecss/Types.h"

namespace SFE::ComponentsModule {
	// One kind of litter: a model, and how it behaves as a loose body.
	struct GarbageItem {
		std::string model;
		// Metres along its longest side: models come in whatever units they were exported in.
		float size = 0.3f;
		float mass = 0.3f; // kg
		// Jolt's buoyancy: under 1 it sinks, over 1 it floats. 0 never goes on the water.
		float buoyancy = 0.f;
		// How often it is picked, relative to the others.
		float weight = 1.f;
	};

	// Litter strewn along the lake's bank and floating on it, every piece a loose physics body the character can push,
	// kick, carry and throw. Placed from the seed around the scene's terrain and water when GarbageSystem starts.
	struct GarbageComponent {
		std::vector<GarbageItem> items;
		uint32_t seed = 1;
		// Pieces on the bank: where the ground is from `shoreBelow` under the water line to `shoreAbove` over it.
		uint32_t shoreCount = 2000;
		float shoreBelow = 0.3f;
		float shoreAbove = 1.5f;
		// Pieces on the water, where it is at least `floatDepth` deep; only items that float.
		uint32_t floatCount = 250;
		float floatDepth = 0.8f;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::GarbageComponent> : std::true_type {};

using SFE::ComponentsModule::GarbageComponent;
