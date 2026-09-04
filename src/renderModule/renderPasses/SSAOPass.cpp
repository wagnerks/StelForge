#include "SSAOPass.h"

#include <algorithm>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "renderModule/Utils.h"
#include "debugModule/Benchmark.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;

void SSAOPass::init(SystemsModule::RenderData& renderDataHandle) {
	const int fullW = (std::max)(1, Engine::instance()->getWindow()->getScreenData().renderW);
	const int fullH = (std::max)(1, Engine::instance()->getWindow()->getScreenData().renderH);
	mSsaoW = (std::max)(1, fullW / 2);
	mSsaoH = (std::max)(1, fullH / 2);
	auto* rhi = Engine::instance()->rhi();

	TextureDesc ssaoColorDesc;
	ssaoColorDesc.width = mSsaoW;
	ssaoColorDesc.height = mSsaoH;
	ssaoColorDesc.format = TextureFormat::R8;
	ssaoColorDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mData.mSsaoColorBuffer = rhi->createTexture(ssaoColorDesc);
	mData.mSsaoColorBufferBlur = rhi->createTexture(ssaoColorDesc);

	RenderPassDesc mainRPD;
	mainRPD.colorAttachmentCount = 1;
	mainRPD.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	mainRPD.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	mainRPD.colorAttachments[0].clearColor = {1.f, 1.f, 1.f, 1.f};
	mainRPD.colorAttachments[0].format = TextureFormat::R8;
	main = rhi->createRenderPass(mainRPD);

	RenderPassDesc blurRPD;
	blurRPD.colorAttachmentCount = 1;
	blurRPD.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	blurRPD.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	blurRPD.colorAttachments[0].clearColor = {1.f, 1.f, 1.f, 1.f};
	blurRPD.colorAttachments[0].format = TextureFormat::R8;
	blur = rhi->createRenderPass(blurRPD);

	FramebufferDesc fbDesc;
	fbDesc.width = mSsaoW;
	fbDesc.height = mSsaoH;
	fbDesc.renderPass = main;
	fbDesc.colorAttachments = {{mData.mSsaoColorBuffer}};
	mData.mSsaoFbo = rhi->createFramebuffer(fbDesc);

	FramebufferDesc blurFbDesc;
	blurFbDesc.width = mSsaoW;
	blurFbDesc.height = mSsaoH;
	blurFbDesc.renderPass = blur;
	blurFbDesc.colorAttachments = {{mData.mSsaoColorBufferBlur}};
	mData.mSsaoBlurFbo = rhi->createFramebuffer(blurFbDesc);

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	mData.sampler = rhi->createSampler(samplerDesc);

	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	mainTexSetLayout = rhi->createDescriptorSetLayout(texLayoutDesc);
	mainTexSet = rhi->allocateDescriptorSet(mainTexSetLayout);

	Render::DescriptorSetLayoutDesc texLayoutBlurDesc;
	texLayoutBlurDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutBlurDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutBlurDesc.bindings.emplace_back(2, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	blurTexSetLayout = rhi->createDescriptorSetLayout(texLayoutBlurDesc);
	blurTexSet = rhi->allocateDescriptorSet(blurTexSetLayout);

	GraphicsPipelineDesc mainPipelineDesc;
	Render::VertexLayoutDesc& layout = mainPipelineDesc.layout;
	layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	mainPipelineDesc.topology = PrimitiveTopology::Triangles;
	mainPipelineDesc.cullMode = CullMode::None;
	mainPipelineDesc.renderPass = main;
	mainPipelineDesc.descriptorSetLayouts = { mainTexSetLayout };
	mainPipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/ssao.vs");
	mainPipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/ssao.fs");
	mainPipeline = rhi->createGraphicsPipeline(mainPipelineDesc);

	GraphicsPipelineDesc blurPipelineDesc;
	blurPipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	blurPipelineDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	blurPipelineDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	blurPipelineDesc.topology = PrimitiveTopology::Triangles;
	blurPipelineDesc.cullMode = CullMode::None;
	blurPipelineDesc.renderPass = blur;
	blurPipelineDesc.descriptorSetLayouts = { blurTexSetLayout };
	blurPipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/ssao.vs");
	blurPipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/ssao_blur.fs");
	blurPipeline = rhi->createGraphicsPipeline(blurPipelineDesc);
	BufferDesc ssaoBufferDesc;
	ssaoBufferDesc.usage = BufferUsage::Uniform;
	ssaoBufferDesc.size = sizeof(SSAOParams);
	ssaoBufferDesc.cpuAccess = CpuAccess::Write;
	ssaoParamsBuffer = rhi->createBuffer(ssaoBufferDesc, &params);

	BufferDesc ssaoBlurBufferDesc;
	ssaoBlurBufferDesc.usage = BufferUsage::Uniform;
	ssaoBlurBufferDesc.size = sizeof(SSAOBlurParams);
	ssaoBlurBufferDesc.cpuAccess = CpuAccess::Write;

	blurParams.sigmaS = mData.sigmaS;
	blurParams.sigmaL = mData.sigmaL;
	blurParams.updateFac();

	ssaoBlurParamsBuffer = rhi->createBuffer(ssaoBlurBufferDesc, &blurParams);

	const auto gPos = renderDataHandle.frame.texture(FrameId::GBufferPosition);
	const auto gPosSamp = renderDataHandle.frame.sampler(FrameId::GBufferPosition);
	const auto gNrm = renderDataHandle.frame.texture(FrameId::GBufferNormal);
	const auto gNrmSamp = renderDataHandle.frame.sampler(FrameId::GBufferNormal);

	std::vector<Render::WriteDescriptor> writes;

	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 0;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ gPos, gPosSamp });
		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 1;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ gNrm, gNrmSamp });
		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 3;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ ssaoParamsBuffer });
		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = blurTexSet;
		w.binding = 0;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ mData.mSsaoColorBuffer, mData.sampler });
		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = blurTexSet;
		w.binding = 1;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ gPos, gPosSamp });
		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = blurTexSet;
		w.binding = 2;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ ssaoBlurParamsBuffer });
		writes.push_back(std::move(w));
	}

	rhi->updateDescriptorSets(writes);

	renderDataHandle.frame.set(FrameId::SsaoColor, { .texture = mData.mSsaoColorBuffer, .sampler = mData.sampler });
	renderDataHandle.frame.set(FrameId::SsaoBlur, { .texture = mData.mSsaoColorBufferBlur, .sampler = mData.sampler });
}

void SSAOPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	if (!renderDataHandle.graphics.ssao) {
		return;
	}
	FUNCTION_BENCHMARK

	auto* rhi = Engine::instance()->rhi();
	rhi->updateBuffer(ssaoParamsBuffer, params);
	rhi->updateBuffer(ssaoBlurParamsBuffer, blurParams);

	if (renderDataHandle.cameraMatricesUBO) {
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 5;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		rhi->updateDescriptorSets({ std::move(w) });
	}

	const int w = mSsaoW > 0 ? mSsaoW : (std::max)(1, Engine::instance()->getWindow()->getScreenData().renderW / 2);
	const int h = mSsaoH > 0 ? mSsaoH : (std::max)(1, Engine::instance()->getWindow()->getScreenData().renderH / 2);

	RenderPassBeginInfo beginInfo;
	beginInfo.width = w;
	beginInfo.height = h;
	beginInfo.framebuffer = mData.mSsaoFbo;
	beginInfo.renderPass = main;
	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, mainPipeline);
	rhi->cmdBindDescriptorSets(cmd, mainPipeline, 0, &mainTexSet, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	beginInfo.framebuffer = mData.mSsaoBlurFbo;
	beginInfo.renderPass = blur;
	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, blurPipeline);
	rhi->cmdBindDescriptorSets(cmd, blurPipeline, 0, &blurTexSet, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);
}
