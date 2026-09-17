#pragma once

#include <optional>

#include "componentsModule/SkyComponent.h"
#include "componentsModule/WindComponent.h"
#include "rhi/RenderHardwareInterface.h"

namespace SFE::SystemsModule {
	struct RenderData;
}

// The atmosphere of shaders/sfe_sky.glsl on the CPU, for what is computed once instead of per pixel: the
// ambient light baked from the sky and the colour of the sunlight. Keep the constants in step with the shader.
namespace SFE::Render::Atmosphere {
	// How bright the sky is for a sun of unit intensity.
	constexpr float kSkyBrightness = 2.4f;
	// How bright the sky is under a full moon against the same sky at noon, and its colour: everything that reads the
	// sky -- the ambient light, the water, the haze -- then has something to work with at night.
	constexpr float kMoonlight = 0.0016f;

	// Sky radiance along dir for a sun of unit intensity towards toSun.
	Math::Vec3 radiance(const Math::Vec3& dir, const Math::Vec3& toSun);
	// The fraction of sunlight left after crossing the atmosphere towards toSun.
	Math::Vec3 sunTransmittance(const Math::Vec3& toSun);

	// radiance() over every direction above the horizon for the scene's sun, so a shader looks the sky up instead of
	// marching it for every pixel: u the azimuth, atan2(z, x) / 2pi + 0.5, v the square root of the direction's
	// height. sfeSkyRadiance in sfe_sky.glsl reads it. Baked again, a few times a second at most, when the sun
	// moves. Render thread.
	struct SkyTexture {
		TextureHandle texture;
		SamplerHandle sampler;
	};
	SkyTexture skyTexture();
	// The cloud layer baked by scripts/cloud_texture.py, repeating; empty until it has loaded.
	SkyTexture cloudTexture();

	// Mirrors SFE_SKY_PARAMS in sfe_sky.glsl. std140.
	struct SkyParams {
		Math::Mat4 invViewProj;
		float toSun[4]{ 0.f, 1.f, 0.f, 0.f };
		float sunColor[4]{};
		float cameraPos[4]{};
		float wind[4]{}; // xz the direction the wind blows along, y its strength
	};
	// False when the scene has no sky.
	bool skyParams(const SystemsModule::RenderData& renderData, SkyParams& out);
	// The scene's sky, if it has one.
	// With `withSettings` the time of day from the graphics settings places its sun, when they set one.
	std::optional<ComponentsModule::SkyComponent> sceneSky(bool withSettings = true);
	// The scene's wind, if it has one; with `withSettings` its strength from the graphics settings, when they set one.
	std::optional<ComponentsModule::WindComponent> sceneWind(bool withSettings = true);
}
