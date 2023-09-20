#pragma once

enum eSystemsPriority : uint16_t {
	DEFAULT = 0,
	TRANSFORM_SYSTEM,
	LOD_SYSTEM,
	SHADERS_SYSTEM,
	RENDER_SYSTEM
};

enum eRenderPassPriority : size_t {
	CASCADE_SHADOWS = 0,
	GEOMETRY,
	SHADERS,
	SSAO,
	LIGHTING,
};