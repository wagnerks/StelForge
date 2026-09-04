#include "TAAPass.h"

#include "assetsModule/modelModule/Vertex.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

void TAAPass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	SFE::Render::SamplerDesc lin;
	lin.minFilter = SFE::Render::FilterMode::Linear;
	lin.magFilter = SFE::Render::FilterMode::Linear;
	mLinear = rhi->createSampler(lin);
	SFE::Render::SamplerDesc nearS;
	nearS.minFilter = SFE::Render::FilterMode::Nearest;
	nearS.magFilter = SFE::Render::FilterMode::Nearest;
	mNearest = rhi->createSampler(nearS);

	SFE::Render::RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = SFE::Render::AttachmentLoadOp::Clear;
	rpDesc.colorAttachments[0].storeOp = SFE::Render::AttachmentStoreOp::Store;
	rpDesc.colorAttachments[0].format = SFE::Render::TextureFormat::RGBA16F;
	mRenderPass = rhi->createRenderPass(rpDesc);

	SFE::Render::TextureDesc texDesc;
	texDesc.width = w;
	texDesc.height = h;
	texDesc.format = SFE::Render::TextureFormat::RGBA16F;
	texDesc.usage = SFE::Render::TextureUsage::ColorAttachment | SFE::Render::TextureUsage::Sampled;
	for (int i = 0; i < 2; ++i) {
		mHistory[i] = rhi->createTexture(texDesc);
		SFE::Render::FramebufferDesc fb;
		fb.width = w;
		fb.height = h;
		fb.renderPass = mRenderPass;
		fb.colorAttachments = { { mHistory[i] } };
		mFbo[i] = rhi->createFramebuffer(fb);
	}

	SFE::Render::DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(2, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, SFE::Render::DescriptorType::UniformBuffer, SFE::Render::ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mLayout);

	SFE::Render::GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(SFE::Vertex2D)), SFE::Render::VertexInputRate::PerVertex);
	pipelineDesc.layout.attributes.emplace_back(0, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, position)), false);
	pipelineDesc.layout.attributes.emplace_back(1, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, texCoords)), false);
	pipelineDesc.topology = SFE::Render::PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = SFE::Render::CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.renderPass = mRenderPass;
	pipelineDesc.descriptorSetLayouts = { mLayout };
	pipelineDesc.shaders.vertex = rhi->loadShader(SFE::Render::ShaderStage::Vertex, "shaders/deferred_shading.vs");
	pipelineDesc.shaders.fragment = rhi->loadShader(SFE::Render::ShaderStage::Fragment, "shaders/taa.fs");
	mPipeline = rhi->createGraphicsPipeline(pipelineDesc);

	SFE::Render::BufferDesc uboDesc;
	uboDesc.usage = SFE::Render::BufferUsage::Uniform;
	uboDesc.cpuAccess = SFE::Render::CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resX = static_cast<float>(w);
	mParams.resY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);
}

void TAAPass::render(SystemsModule::RenderData& renderDataHandle, SFE::Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	auto& frame = renderDataHandle.frame;
	const auto src = frame.texture(SFE::Render::FrameId::Final);
	const auto pos = frame.texture(SFE::Render::FrameId::GBufferPosition);
	if (!renderDataHandle.graphics.taa) {
		mHasHistory = false;
		return;
	}
	if (!src || !pos || !mPipeline) {
		return;
	}

	mParams.prevPV = renderDataHandle.prevPV;
	mParams.blend = renderDataHandle.graphics.taaBlend;
	mParams.reset = mHasHistory ? 0 : 1;
	{
		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		mParams.resX = screen.renderW > 0 ? static_cast<float>(screen.renderW) : 1.f;
		mParams.resY = screen.renderH > 0 ? static_cast<float>(screen.renderH) : 1.f;
	}
	rhi->updateBuffer(mParamsUBO, mParams);

	const int hist = 1 - mWrite;
	std::vector<SFE::Render::WriteDescriptor> writes;
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mSet;
		w.binding = 0;
		w.type = SFE::Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ src, mLinear });
		writes.push_back(std::move(w));
	}
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mSet;
		w.binding = 1;
		w.type = SFE::Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ mHistory[hist], mLinear });
		writes.push_back(std::move(w));
	}
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mSet;
		w.binding = 2;
		w.type = SFE::Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ pos, frame.sampler(SFE::Render::FrameId::GBufferPosition) ? frame.sampler(SFE::Render::FrameId::GBufferPosition) : mNearest });
		writes.push_back(std::move(w));
	}
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mSet;
		w.binding = 15;
		w.type = SFE::Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ mParamsUBO });
		writes.push_back(std::move(w));
	}
	rhi->updateDescriptorSets(writes);

	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	SFE::Render::RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mFbo[mWrite];
	beginInfo.renderPass = mRenderPass;
	beginInfo.width = screen.renderW;
	beginInfo.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, mPipeline);
	rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mSet, 1);
	SFE::Render::Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	frame.set(SFE::Render::FrameId::Final, { .texture = mHistory[mWrite], .sampler = mLinear, .framebuffer = mFbo[mWrite] });
	mWrite = hist;
	mHasHistory = true;
}
