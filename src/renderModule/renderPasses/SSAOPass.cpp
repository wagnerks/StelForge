#include "SSAOPass.h"

#include <random>
#include <gtc/random.hpp>

#include "imgui.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "debugModule/Benchmark.h"
#include "logsModule/logger.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;

float lerp(float a, float b, float f) {
	return a + f * (b - a);
}

void SSAOPass::init(SystemsModule::RenderData& renderDataHandle) {

	renderDataHandle.mSSAOPassData = &mData;

	// SSAO color buffer
	TextureDesc ssaoColorDesc;
	ssaoColorDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	ssaoColorDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	ssaoColorDesc.format = TextureFormat::R8;
	ssaoColorDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mData.mSsaoColorBuffer = Engine::instance()->rhi()->createTexture(ssaoColorDesc);

	TextureDesc ssaoColorBlurDesc;
	ssaoColorBlurDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	ssaoColorBlurDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	ssaoColorBlurDesc.format = TextureFormat::R8;
	ssaoColorBlurDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mData.mSsaoColorBufferBlur = Engine::instance()->rhi()->createTexture(ssaoColorBlurDesc);

	FramebufferDesc fbDesc;
	fbDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	fbDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	fbDesc.colorAttachments = {{mData.mSsaoColorBuffer}};
	mData.mSsaoFbo = Engine::instance()->rhi()->createFramebuffer(fbDesc);

	// and blur stage
	FramebufferDesc fbBlurDesc;
	fbBlurDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	fbBlurDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	fbBlurDesc.colorAttachments = { {mData.mSsaoColorBufferBlur} };
	mData.mSsaoBlurFbo = Engine::instance()->rhi()->createFramebuffer(fbBlurDesc);

	// generate sample kernel
	// ----------------------

	std::random_device rd;
	std::mt19937 generator(rd());

	// Define the distribution for the range -5.0f to 15.0f
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distribution2(0.0f, 1.0f);

	for (unsigned int i = 0; i < 64; ++i) {
		Math::Vec3 sample(distribution(generator), distribution(generator), distribution2(generator));
		sample = Math::normalize(Math::Vec3{ sample.x, sample.y, sample.z });
		sample *= distribution(generator);
		float scale = float(i) / 64.0f;

		// scale samples s.t. they're more aligned to center of kernel
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		mData.mSsaoKernel.push_back(sample);
	}

	// generate noise texture
	// ----------------------
	std::vector<Math::Vec3> ssaoNoise;
	for (unsigned int i = 0; i < 16; i++) {
		Math::Vec3 noise(distribution(generator), distribution(generator), 0.0f);
		// rotate around z-axis (in tangent space)
		ssaoNoise.push_back(noise);
	}
	TextureDesc noiseDesc;
	noiseDesc.width = 4;
	noiseDesc.height = 4;
	noiseDesc.format = TextureFormat::RGB32F;
	noiseDesc.usage = TextureUsage::Sampled;

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Nearest;
	samplerDesc.magFilter = FilterMode::Nearest;

	mData.sampler = Engine::instance()->rhi()->createSampler(samplerDesc);

	mData.mNoiseTexture = Engine::instance()->rhi()->createTexture(noiseDesc);

	GraphicsPipelineDesc mainPipelineDesc;
	Render::VertexLayoutDesc& layout = mainPipelineDesc.layout;
	layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	mainPipelineDesc.topology = PrimitiveTopology::Triangles;
	mainPipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "ssao_first_stage_vs", ShaderModule::ShaderBase::loadShaderCode("shaders/ssao.vs") });
	mainPipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Fragment, "ssao_first_stage_fs", ShaderModule::ShaderBase::loadShaderCode("shaders/ssao.fs")});
	mainPipeline = Engine::instance()->rhi()->createGraphicsPipeline(mainPipelineDesc);

	RenderPassDesc mainRPD;
	mainRPD.colorAttachmentCount = 1;
	mainRPD.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	mainRPD.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	main = Engine::instance()->rhi()->createRenderPass(mainRPD);

	GraphicsPipelineDesc blurPipelineDesc;
	blurPipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	blurPipelineDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	blurPipelineDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	blurPipelineDesc.topology = PrimitiveTopology::Triangles;
	blurPipelineDesc.shaders.vertex = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "ssao_blur_stage_vs", ShaderModule::ShaderBase::loadShaderCode("shaders/ssao.vs") });
	blurPipelineDesc.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Fragment, "ssao_blur_stage_fs", ShaderModule::ShaderBase::loadShaderCode("shaders/ssao_blur.fs") });
	blurPipeline = Engine::instance()->rhi()->createGraphicsPipeline(blurPipelineDesc);

	BufferDesc ssaoBufferDesc;
	ssaoBufferDesc.usage = BufferUsage::Uniform;
	ssaoBufferDesc.size = sizeof(SSAOParams);
	ssaoBufferDesc.cpuAccess = CpuAccess::Write;
	ssaoParamsBuffer = Engine::instance()->rhi()->createBuffer(ssaoBufferDesc, &params);

	BufferDesc ssaoBlurBufferDesc;
	ssaoBlurBufferDesc.usage = BufferUsage::Uniform;
	ssaoBlurBufferDesc.size = sizeof(SSAOBlurParams);
	ssaoBlurBufferDesc.cpuAccess = CpuAccess::Write;

	blurParams.sigmaS = mData.sigmaS;
	blurParams.sigmaL = mData.sigmaL;
	blurParams.updateFac();

	ssaoBlurParamsBuffer = Engine::instance()->rhi()->createBuffer(ssaoBlurBufferDesc, &blurParams);

	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);

	mainTexSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(texLayoutDesc);
	mainTexSet = Engine::instance()->rhi()->allocateDescriptorSet(mainTexSetLayout);

	Render::DescriptorSetLayoutDesc texLayoutBlurDesc;
	texLayoutBlurDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutBlurDesc.bindings.emplace_back(1, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);

	blurTexSetLayout = Engine::instance()->rhi()->createDescriptorSetLayout(texLayoutBlurDesc);
	blurTexSet = Engine::instance()->rhi()->allocateDescriptorSet(blurTexSetLayout);

	std::vector<Render::WriteDescriptor> writes;

	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 0;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ renderDataHandle.mGeometryPassData->viewPositionBuffer, renderDataHandle.mGeometryPassData->sampler });

		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 1;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ renderDataHandle.mGeometryPassData->normalBuffer, renderDataHandle.mGeometryPassData->sampler});

		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = mainTexSet;
		w.binding = 2;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ mData.mNoiseTexture, mData.sampler});

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
		w.images.push_back({mData.mSsaoColorBuffer, mData.sampler});

		writes.push_back(std::move(w));
	}
	{
		Render::WriteDescriptor w{};
		w.set = blurTexSet;
		w.binding = 1;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ ssaoBlurParamsBuffer });

		writes.push_back(std::move(w));
	}

	Engine::instance()->rhi()->updateDescriptorSets(writes);
}

void SSAOPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK

	Engine::instance()->rhi()->updateBuffer(ssaoParamsBuffer, params);
	Engine::instance()->rhi()->updateBuffer(ssaoBlurParamsBuffer, blurParams);


	RenderPassBeginInfo beginInfo;
	beginInfo.width = Engine::instance()->getWindow()->getScreenData().renderW;
	beginInfo.height = Engine::instance()->getWindow()->getScreenData().renderH;

	beginInfo.framebuffer = mData.mSsaoFbo;
	beginInfo.renderPass = main;
	Engine::instance()->rhi()->cmdBeginRenderPass(cmd, beginInfo);
	Engine::instance()->rhi()->cmdBindPipeline(cmd, mainPipeline);
	Engine::instance()->rhi()->cmdBindDescriptorSets(cmd, mainPipeline, 0, &mainTexSet, 1);
	Utils::renderQuad(cmd);
	Engine::instance()->rhi()->cmdEndRenderPass(cmd);

	beginInfo.framebuffer = mData.mSsaoBlurFbo;
	beginInfo.renderPass = main;
	Engine::instance()->rhi()->cmdBeginRenderPass(cmd, beginInfo);
	Engine::instance()->rhi()->cmdBindPipeline(cmd, blurPipeline);
	Engine::instance()->rhi()->cmdBindDescriptorSets(cmd, blurPipeline, 0, &blurTexSet, 1);
	Utils::renderQuad(cmd);
	Engine::instance()->rhi()->cmdEndRenderPass(cmd);
}
