#pragma once

#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/DebugPass.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/GUIPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/ShadersPass.h"
#include "renderModule/renderPasses/SSAOPass.h"

//the pass position in array is the pass draw priority, insert passes to this list
const static inline std::array RENDER_PASSES_PRIORITY {
	typeid(SFE::Render::RenderPasses::CascadedShadowPass).hash_code(),
	typeid(SFE::Render::RenderPasses::PointLightPass).hash_code(),
	typeid(SFE::Render::RenderPasses::GeometryPass).hash_code(),
	typeid(SFE::Render::RenderPasses::ShadersPass).hash_code(),
	typeid(SFE::Render::RenderPasses::SSAOPass).hash_code(),
	typeid(SFE::Render::RenderPasses::LightingPass).hash_code(),
	typeid(SFE::Render::RenderPasses::DebugPass).hash_code(),
	typeid(SFE::Render::RenderPasses::GUIPass).hash_code(),
};
