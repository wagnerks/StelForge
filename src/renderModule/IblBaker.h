#pragma once
#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render {
	struct IblMaps {
		TextureHandle irradiance;
		TextureHandle prefilter;
		TextureHandle brdfLut;
		SamplerHandle irradianceSampler;
		SamplerHandle prefilterSampler;
		SamplerHandle lutSampler;
	};

	IblMaps bakeIbl(RenderHardwareInterface* rhi);
}
