#pragma once

#include <string>

#include "systemsModule/systems/RenderSystem.h"

namespace Json { class Value; }

namespace SFE::SystemsModule {
	bool drawGraphicsSettings(GraphicsSettings& gfx);
	// Time of day and wind, over the scene's own until moved. True when one changed.
	bool environmentSliders(GraphicsSettings& gfx);
	// AudioSystem's master volume.
	void volumeSlider();
	// The player character's CharacterAppearanceComponent: skin and hair colour, hairstyle.
	void characterAppearance();
	std::string graphicsToJson(const GraphicsSettings& gfx);
	void graphicsFromJson(const Json::Value& in, GraphicsSettings& gfx);
	bool graphicsFromJson(const std::string& json, GraphicsSettings& gfx);
}
