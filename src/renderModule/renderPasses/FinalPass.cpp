#include "FinalPass.h"

#include <algorithm>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/modelModule/Vertex.h"
#include "core/Engine.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

SFE::Render::PipelineHandle FinalPass::makeFullscreen(SFE::Render::RenderHardwareInterface* rhi, const char* vsName, const char* fsName, const std::string& vs, const std::string& fs) {
	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	pipelineDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	pipelineDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, vsName, vs });
	pipelineDesc.shaders.fragment = rhi->createShaderModule({ ShaderStage::Fragment, fsName, fs });
	return rhi->createGraphicsPipeline(pipelineDesc);
}

void FinalPass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	mSampler = rhi->createSampler(samplerDesc);

	const auto vs = SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/deferred_shading.vs");
	mBloomPipeline = makeFullscreen(rhi, "bloom vs", "bloom fs", vs, SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/bloom_extract.fs"));
	mPostPipeline = makeFullscreen(rhi, "post vs", "post fs", vs, SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/post.fs"));

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	rpDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	mRenderPass = rhi->createRenderPass(rpDesc);

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, DescriptorType::UniformBuffer, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mBloomSet = rhi->allocateDescriptorSet(mLayout);
	mPostSet = rhi->allocateDescriptorSet(mLayout);

	BufferDesc postUbo;
	postUbo.usage = BufferUsage::Uniform;
	postUbo.cpuAccess = CpuAccess::Write;
	postUbo.size = sizeof(PostParams);
	mPostUBO = rhi->createBuffer(postUbo, &mPostParams);

	TextureDesc bloomDesc;
	bloomDesc.width = std::max(1, w / 4);
	bloomDesc.height = std::max(1, h / 4);
	bloomDesc.format = TextureFormat::RGBA16F;
	bloomDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mBloomTex = rhi->createTexture(bloomDesc);
	FramebufferDesc bloomFb;
	bloomFb.width = bloomDesc.width;
	bloomFb.height = bloomDesc.height;
	bloomFb.colorAttachments = { { mBloomTex } };
	mBloomFbo = rhi->createFramebuffer(bloomFb);

	TextureDesc displayDesc;
	displayDesc.width = w;
	displayDesc.height = h;
	displayDesc.format = TextureFormat::RGBA8;
	displayDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mDisplayTex = rhi->createTexture(displayDesc);
	FramebufferDesc displayFb;
	displayFb.width = w;
	displayFb.height = h;
	displayFb.colorAttachments = { { mDisplayTex } };
	mDisplayFbo = rhi->createFramebuffer(displayFb);
}

void FinalPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	const auto src = renderDataHandle.frame.texture(FrameId::Final);
	if (!src || !mPostPipeline || !mBloomPipeline) {
		return;
	}

	auto* rhi = Engine::instance()->rhi();
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	auto& gfx = renderDataHandle.graphics;

	mPostParams.bloomStrength = gfx.bloom ? gfx.bloomStrength : 0.f;
	mPostParams.exposure = gfx.exposure;
	mPostParams.vignette = gfx.vignette;
	mPostParams.ca = gfx.chromaticAberration;
	rhi->updateBuffer(mPostUBO, mPostParams);

	if (gfx.bloom) {
		WriteDescriptor w{};
		w.set = mBloomSet;
		w.binding = 0;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ src, mSampler });
		rhi->updateDescriptorSets({ w });

		RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mBloomFbo;
		beginInfo.renderPass = mRenderPass;
		beginInfo.width = std::max(1, screen.renderW / 4);
		beginInfo.height = std::max(1, screen.renderH / 4);
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mBloomPipeline);
		rhi->cmdBindDescriptorSets(cmd, mBloomPipeline, 0, &mBloomSet, 1);
		Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}

	{
		std::vector<WriteDescriptor> writes;
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 0;
			w.type = DescriptorType::CombinedImageSampler;
			w.images.push_back({ src, mSampler });
			writes.push_back(std::move(w));
		}
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 1;
			w.type = DescriptorType::CombinedImageSampler;
			w.images.push_back({ mBloomTex, mSampler });
			writes.push_back(std::move(w));
		}
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 15;
			w.type = DescriptorType::UniformBuffer;
			w.buffers.push_back({ mPostUBO });
			writes.push_back(std::move(w));
		}
		rhi->updateDescriptorSets(writes);

		RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mDisplayFbo;
		beginInfo.renderPass = mRenderPass;
		beginInfo.width = screen.renderW;
		beginInfo.height = screen.renderH;
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mPostPipeline);
		rhi->cmdBindDescriptorSets(cmd, mPostPipeline, 0, &mPostSet, 1);
		Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}

	renderDataHandle.frame.set(FrameId::Final, { .texture = mDisplayTex, .sampler = mSampler, .framebuffer = mDisplayFbo });

	if (Engine::instance()->presentToScreen()) {
		rhi->cmdBlit(cmd, mDisplayFbo, rhi->getBackbufferFramebuffer(), BlitMask::Color, BlitFilter::Nearest);
	}
}
