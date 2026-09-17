#pragma once

#include <cmath>

#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	// The wind over an outdoor scene: it sways the vegetation, ruffles the water and drives the clouds.
	struct WindComponent {
		float direction = 45.f; // degrees around the vertical axis: 0 blows towards +z, 90 towards +x
		float strength = 0.5f;  // 0 still air, 1 a strong breeze

		// The unit vector on the ground the wind blows along.
		Math::Vec2 blowing() const {
			const float radians = direction * 0.0174533f;
			return { std::sin(radians), std::cos(radians) };
		}
	};

	// How a surface moves in the wind.
	enum class Sway : uint8_t {
		None,
		Tree,  // bends from its base, more the taller it is, with a light flutter of the crown
		Plant  // grass, flowers, bushes: soft and quick, from the ground up
	};
}

using SFE::ComponentsModule::WindComponent;
