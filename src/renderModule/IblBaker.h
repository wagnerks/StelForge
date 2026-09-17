#pragma once
#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	struct IblMaps {
		TextureHandle irradiance;
		TextureHandle prefilter;
		TextureHandle brdfLut;
		SamplerHandle irradianceSampler;
		SamplerHandle prefilterSampler;
		SamplerHandle lutSampler;
	};

	// The scene's sky when it has one, the studio environment otherwise.
	IblMaps bakeIbl(RenderHardwareInterface* rhi, float sunIntensity);
}
