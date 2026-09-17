#include "SceneDepthPass.h"

#include <algorithm>

#include "assetsModule/modelModule/Vertex.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

void SceneDepthPass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();

	SamplerDesc nearest;
	nearest.minFilter = FilterMode::Nearest;
	nearest.magFilter = FilterMode::Nearest;
	mNearest = rhi->createSampler(nearest);

	RenderPassDesc passDesc;
	passDesc.colorAttachmentCount = 1;
	passDesc.colorAttachments[0] = { AttachmentLoadOp::DontCare, AttachmentStoreOp::Store, { 1.f, 0.f, 0.f, 0.f }, TextureFormat::R32F };
	mRenderPass = rhi->createRenderPass(passDesc);

	TextureDesc depthDesc;
	depthDesc.width = screen.renderW;
	depthDesc.height = screen.renderH;
	depthDesc.format = TextureFormat::R32F;
	depthDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mDepth = rhi->createTexture(depthDesc);
	FramebufferDesc fb;
	fb.width = screen.renderW;
	fb.height = screen.renderH;
	fb.renderPass = mRenderPass;
	fb.colorAttachments = { { mDepth } };
	mFbo = rhi->createFramebuffer(fb);

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	for (uint32_t i = 0; i < (std::max)(1u, rhi->framesInFlight()); ++i) {
		mSets.push_back(rhi->allocateDescriptorSet(mLayout));
	}

	GraphicsPipelineDesc desc;
	desc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	desc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, position)), false);
	desc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, texCoords)), false);
	desc.topology = PrimitiveTopology::Triangles;
	desc.cullMode = CullMode::None;
	desc.depthStencilState.depthTest = false;
	desc.depthStencilState.depthWrite = false;
	desc.renderPass = mRenderPass;
	desc.descriptorSetLayouts = { mLayout };
	desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/deferred_shading.vs");
	desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/scene_depth.fs");
	mPipeline = rhi->createGraphicsPipeline(desc);
}

void SceneDepthPass::render(SystemsModule::RenderData& renderDataHandle, CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	auto* rhi = Engine::instance()->rhi();
	const auto depth = renderDataHandle.frame.texture(FrameId::GBufferDepth);
	if (!depth || !mPipeline) {
		return;
	}
	const auto set = mSets[rhi->frameInFlight() % mSets.size()];
	rhi->updateDescriptorSets({ { .set = set, .binding = 0, .images{ { depth, mNearest } } } });

	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	RenderPassBeginInfo begin;
	begin.framebuffer = mFbo;
	begin.renderPass = mRenderPass;
	begin.width = screen.renderW;
	begin.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, begin);
	rhi->cmdBindPipeline(cmd, mPipeline);
	rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &set, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	renderDataHandle.frame.set(FrameId::SceneDepth, { .texture = mDepth, .sampler = mNearest });
}
