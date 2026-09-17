#pragma once

#include <string>

#include "ecss/Types.h"
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	// The character the player walks around with: its model comes from the entity's ModelComponent, its animations
	// from that model or `animations`, its body and controls from CharacterSystem. Speeds in metres per second, the
	// jump in metres.
	struct CharacterComponent {
		float walkSpeed = 1.7f;
		float runSpeed = 4.2f;
		float sprintSpeed = 6.8f;
		float jumpHeight = 1.0f;
		// The model whose animations it plays, a skeleton with the same bone names; empty: its own model's.
		std::string animations;
	};

	// How a character looks, applied to its model by AppearanceSystem: the parts named "Body" take the skin colour,
	// "Eyebrows" and every "Hair_..." part the hair colour, and of the "Hair_..." parts only `hairStyle` is drawn.
	// Colours are sRGB, 0..1, multiplying the model's own textures.
	struct CharacterAppearanceComponent {
		Math::Vec3 skinColor{ 1.f, 1.f, 1.f };
		Math::Vec3 hairColor{ 0.36f, 0.24f, 0.15f };
		std::string hairStyle = "Hair_SimpleParted";
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::CharacterComponent> : std::true_type {};
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::CharacterAppearanceComponent> : std::true_type {};

using SFE::ComponentsModule::CharacterComponent;
using SFE::ComponentsModule::CharacterAppearanceComponent;
