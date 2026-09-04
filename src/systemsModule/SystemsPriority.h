#pragma once

#include "renderModule/renderPasses/AnimationComputePass.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/DebugPass.h"
#include "renderModule/renderPasses/FinalPass.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/GUIPass.h"
#include "renderModule/renderPasses/LightingPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/RenderPass.h"
#include "renderModule/renderPasses/SSAOPass.h"
#include "renderModule/renderPasses/SSRPass.h"
#include "renderModule/renderPasses/GlassPass.h"
#include "renderModule/renderPasses/VolumetricPass.h"
#include "renderModule/renderPasses/TAAPass.h"
#include "renderModule/renderPasses/DoFPass.h"
#include "renderModule/renderPasses/TextOverlayPass.h"
#include "renderModule/renderPasses/ImGuiPass.h"
#include "renderModule/renderPasses/SheenPass.h"
#include "renderModule/renderPasses/SSSPass.h"
#include "renderModule/renderPasses/ContactGIPass.h"

namespace SFE::SystemsModule {

	template<typename PassType>
	Render::RenderPass* createRenderPass() {
		return new PassType();
	}

	// Order is draw order. Flip `enabled` to include a pass; insert a row to add one.
	// Vulkan uses the same list; unsupported passes are filtered via RHI::supportsRenderPass().
	inline const Render::RenderPassEntry RENDER_PASSES[] = {
		{ "AnimationCompute", true,  &createRenderPass<Render::RenderPasses::AnimationComputePass> },
		{ "CascadedShadow", true,  &createRenderPass<Render::RenderPasses::CascadedShadowPass> },
		{ "PointLight",     true,  &createRenderPass<Render::RenderPasses::PointLightPass> },
		{ "Geometry",       true,  &createRenderPass<Render::RenderPasses::GeometryPass> },
		{ "SSAO",           true,  &createRenderPass<Render::RenderPasses::SSAOPass> },
		{ "Lighting",       true,  &createRenderPass<Render::RenderPasses::LightingPass> },
		{ "Sheen",          true,  &createRenderPass<Render::RenderPasses::SheenPass> },
		{ "SSS",            true,  &createRenderPass<Render::RenderPasses::SSSPass> },
		{ "ContactGI",      true,  &createRenderPass<Render::RenderPasses::ContactGIPass> },
		{ "SSR",            true,  &createRenderPass<Render::RenderPasses::SSRPass> },
		{ "Glass",          true,  &createRenderPass<Render::RenderPasses::GlassPass> },
		{ "Volumetric",     true,  &createRenderPass<Render::RenderPasses::VolumetricPass> },
		{ "TAA",            true,  &createRenderPass<Render::RenderPasses::TAAPass> },
		{ "DoF",            true,  &createRenderPass<Render::RenderPasses::DoFPass> },
		{ "Final",          true,  &createRenderPass<Render::RenderPasses::FinalPass> },
		{ "GUI",            false, &createRenderPass<Render::RenderPasses::GUIPass> },
		{ "Debug",          true,  &createRenderPass<Render::RenderPasses::DebugPass> },
		{ "TextOverlay",    false, &createRenderPass<Render::RenderPasses::TextOverlayPass> },
		{ "ImGui",          true,  &createRenderPass<Render::RenderPasses::ImGuiPass> },
	};

}
