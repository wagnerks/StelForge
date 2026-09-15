#pragma once

#include <string>

#include "systemsModule/systems/RenderSystem.h"

namespace Json { class Value; }

namespace SFE::SystemsModule {
	// Backend, GPU and the frame time graph.
	void drawFrameStats();
	bool drawGraphicsSettings(GraphicsSettings& gfx);
	std::string graphicsToJson(const GraphicsSettings& gfx);
	void graphicsFromJson(const Json::Value& in, GraphicsSettings& gfx);
	bool graphicsFromJson(const std::string& json, GraphicsSettings& gfx);
}
