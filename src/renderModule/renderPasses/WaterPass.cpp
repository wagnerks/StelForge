#include "WaterPass.h"

#include <algorithm>

#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/WaterComponent.h"
#include "core/ECSHandler.h"
#include "assetsModule/TextureHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/Atmosphere.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

namespace {
	// Mirrors WaterBlock in water.fs. std140.
	struct WaterParams {
		SFE::Render::Atmosphere::SkyParams sky;
		float surface[4]{}; // xz centre, y level, w half size
		float look[4]{};    // x clarity, yz 1 / render size, w 1 when the water looks for reflections on screen
	};
}

void WaterPass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();

	SamplerDesc nearest;
	nearest.minFilter = FilterMode::Nearest;
	nearest.magFilter = FilterMode::Nearest;
	nearest.addressU = AddressMode::ClampToEdge;
	nearest.addressV = AddressMode::ClampToEdge;
	mNearest = rhi->createSampler(nearest);
	SamplerDesc linear = nearest;
	linear.minFilter = FilterMode::Linear;
	linear.magFilter = FilterMode::Linear;
	mLinear = rhi->createSampler(linear);
	SamplerDesc ripple;
	ripple.minFilter = FilterMode::LinearMipmapLinear;
	ripple.magFilter = FilterMode::Linear;
	ripple.addressU = AddressMode::Repeat;
	ripple.addressV = AddressMode::Repeat;
	ripple.maxAnisotropy = 1.f;
	mRipple = rhi->createSampler(ripple);

	TextureDesc copyDesc;
	copyDesc.width = screen.renderW;
	copyDesc.height = screen.renderH;
	copyDesc.format = TextureFormat::RGBA16F;
	copyDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mSceneCopy = rhi->createTexture(copyDesc);
	FramebufferDesc copyFb;
	copyFb.width = screen.renderW;
	copyFb.height = screen.renderH;
	copyFb.colorAttachments = { { mSceneCopy } };
	mSceneCopyFbo = rhi->createFramebuffer(copyFb);

	mSurfacePass = createFinalDepthRenderPass();

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(6, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(7, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, DescriptorType::UniformBufferDynamic, ShaderStage::Vertex | ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(20, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(21, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(22, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	mSurfaceLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSurfaceSets.resize((std::max)(1u, rhi->framesInFlight()));
	for (auto& set : mSurfaceSets) {
		set = rhi->allocateDescriptorSet(mSurfaceLayout);
	}

	GraphicsPipelineDesc desc;
	desc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	desc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, position)), false);
	desc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, texCoords)), false);
	desc.topology = PrimitiveTopology::Triangles;
	desc.cullMode = CullMode::None;
	desc.depthStencilState.depthTest = true;
	desc.depthStencilState.depthWrite = false;
	desc.depthStencilState.depthCompareOp = CompareOp::LessEqual;
	desc.renderPass = mSurfacePass;
	desc.descriptorSetLayouts = { mSurfaceLayout };
	desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/water.vs");
	desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/water.fs");
	mSurfacePipeline = rhi->createGraphicsPipeline(desc);
}

void WaterPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	WaterParams params;
	if (!mSurfacePipeline || !Atmosphere::skyParams(renderDataHandle, params.sky)) {
		return;
	}
	const auto finalFbo = renderDataHandle.frame.framebuffer(FrameId::Final);
	const auto surfaceFbo = mTargets.get(renderDataHandle, mSurfacePass);
	const auto sceneDepth = renderDataHandle.frame.texture(FrameId::SceneDepth);
	if (!finalFbo || !surfaceFbo || !sceneDepth) {
		return;
	}

	// One surface: the first.
	bool found = false;
	for (const auto& [id, water, transform] : ECSHandler::registry().view<const ComponentsModule::WaterComponent, const ComponentsModule::TransformComponent>()) {
		if (!water || !transform) {
			continue;
		}
		const auto pos = transform->getPos(true);
		params.surface[0] = pos.x;
		params.surface[1] = pos.y;
		params.surface[2] = pos.z;
		params.surface[3] = water->size * 0.5f;
		params.look[0] = (std::max)(water->clarity, 0.1f);
		found = true;
		break;
	}
	if (!found) {
		return;
	}
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	params.look[1] = 1.f / static_cast<float>((std::max)(1, screen.renderW));
	params.look[2] = 1.f / static_cast<float>((std::max)(1, screen.renderH));
	// The same switch as the rest of the scene's screen-space reflections: without it the water shows only the sky.
	params.look[3] = renderDataHandle.graphics.ssr ? 1.f : 0.f;

	auto* rhi = Engine::instance()->rhi();
	const auto set = mSurfaceSets[rhi->frameInFlight() % mSurfaceSets.size()];
	const auto block = rhi->allocTransient(params);
	const auto skyLut = Render::Atmosphere::skyTexture();
	const auto clouds = Render::Atmosphere::cloudTexture();
	// The fine ripple map; without it the water keeps the sum of waves alone, which is what it had before.
	const auto* ripple = AssetsModule::TextureHandler::instance()->loadTexture("textures/water/ripple.png");
	rhi->updateDescriptorSets({
		{ .set = set, .binding = 5, .buffers{ { renderDataHandle.cameraMatricesUBO } } },
		{ .set = set, .binding = 6, .images{ { mSceneCopy, mLinear } } },
		{ .set = set, .binding = 7, .images{ { sceneDepth, mNearest } } },
		{ .set = set, .binding = 15, .buffers{ { block.buffer, 0, sizeof(params) } } },
		{ .set = set, .binding = 20, .images{ { skyLut.texture, skyLut.sampler } } },
		{ .set = set, .binding = 21, .images{ { clouds.texture ? clouds.texture : skyLut.texture, clouds.sampler ? clouds.sampler : skyLut.sampler } } },
		{ .set = set, .binding = 22, .images{ { ripple ? ripple->texture : skyLut.texture, ripple ? mRipple : skyLut.sampler } } },
	});
	// The lit scene under and around the water, for the refraction and for what the reflected rays run into.
	rhi->cmdBlit(cmd, finalFbo, mSceneCopyFbo, BlitMask::Color, BlitFilter::Nearest);

	RenderPassBeginInfo begin;
	begin.renderPass = mSurfacePass;
	begin.framebuffer = surfaceFbo;
	begin.width = static_cast<uint32_t>(screen.renderW);
	begin.height = static_cast<uint32_t>(screen.renderH);
	rhi->cmdBeginRenderPass(cmd, begin);
	rhi->cmdBindPipeline(cmd, mSurfacePipeline);
	rhi->cmdBindDescriptorSets(cmd, mSurfacePipeline, 0, &set, 1, &block.offset, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);
}
