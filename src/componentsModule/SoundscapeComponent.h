#pragma once

#include <string>
#include <vector>

#include "ecss/Types.h"

namespace SFE::ComponentsModule {
	// The sound of a step on one kind of ground: recordings of walking on it, cut into single steps where they are played.
	struct FootstepSurface {
		std::string surface; // grass, forest, road, bank or water
		std::vector<std::string> clips;
		float volume = 0.5f;
		float maxLength = 0.45f; // seconds of a step at most
	};

	// The sound of the scene's terrain as the listener moves through it: wind in the trees as loud as the forest around
	// is thick, the lake lapping at the nearest bit of shore, birds singing now and then from trees round about, frogs in
	// the shallows that fall silent when someone comes near. Whatever stands between a sound and the listener -- trunks
	// and crowns -- makes it quieter and duller. Played by SoundscapeSystem.
	struct SoundscapeComponent {
		std::string wind;               // a loop
		float windVolume = 0.3f;        // deep in the forest in a full wind
		std::string lake;               // a loop
		float lakeVolume = 0.35f;       // at the water's edge
		std::vector<std::string> birds; // single songs
		float birdVolume = 0.35f;
		std::vector<std::string> frogs; // loops
		float frogVolume = 0.45f;
		uint32_t seed = 1;
		// The character's steps, by what the ground under its feet is.
		std::vector<FootstepSurface> footsteps;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::SoundscapeComponent> : std::true_type {};

using SFE::ComponentsModule::SoundscapeComponent;
