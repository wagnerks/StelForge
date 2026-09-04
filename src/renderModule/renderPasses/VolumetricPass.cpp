#include "VolumetricPass.h"

#include <algorithm>

#include "assetsModule/modelModule/Vertex.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

namespace {
	SFE::Render::PipelineHandle makeFs(SFE::Render::RenderHardwareInterface* rhi, SFE::Render::RenderPassHandle renderPass,
		SFE::Render::DescriptorSetLayoutHandle layout, const char* vsPath, const char* fsPath) {
		SFE::Render::GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(SFE::Vertex2D)), SFE::Render::VertexInputRate::PerVertex);
		pipelineDesc.layout.attributes.emplace_back(0, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, position)), false);
		pipelineDesc.layout.attributes.emplace_back(1, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, texCoords)), false);
		pipelineDesc.topology = SFE::Render::PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = SFE::Render::CullMode::None;
		pipelineDesc.depthStencilState.depthTest = false;
		pipelineDesc.depthStencilState.depthWrite = false;
		pipelineDesc.renderPass = renderPass;
		pipelineDesc.descriptorSetLayouts = { layout };
		pipelineDesc.shaders.vertex = rhi->loadShader(SFE::Render::ShaderStage::Vertex, vsPath);
		pipelineDesc.shaders.fragment = rhi->loadShader(SFE::Render::ShaderStage::Fragment, fsPath);
		return rhi->createGraphicsPipeline(pipelineDesc);
	}
}

void VolumetricPass::init(SystemsModule::RenderData&) {
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

	SFE::Render::TextureDesc halfDesc;
	halfDesc.width = static_cast<uint32_t>(std::max(1, w / 2));
	halfDesc.height = static_cast<uint32_t>(std::max(1, h / 2));
	halfDesc.format = SFE::Render::TextureFormat::RGBA16F;
	halfDesc.usage = SFE::Render::TextureUsage::ColorAttachment | SFE::Render::TextureUsage::Sampled;
	mHalfTex = rhi->createTexture(halfDesc);
	SFE::Render::FramebufferDesc halfFb;
	halfFb.width = halfDesc.width;
	halfFb.height = halfDesc.height;
	halfFb.renderPass = mRenderPass;
	halfFb.colorAttachments = { { mHalfTex } };
	mHalfFbo = rhi->createFramebuffer(halfFb);

	SFE::Render::TextureDesc colorDesc;
	colorDesc.width = w;
	colorDesc.height = h;
	colorDesc.format = SFE::Render::TextureFormat::RGBA16F;
	colorDesc.usage = SFE::Render::TextureUsage::ColorAttachment | SFE::Render::TextureUsage::Sampled;
	mColor = rhi->createTexture(colorDesc);
	SFE::Render::FramebufferDesc fb;
	fb.width = w;
	fb.height = h;
	fb.renderPass = mRenderPass;
	fb.colorAttachments = { { mColor } };
	mFbo = rhi->createFramebuffer(fb);

	SFE::Render::BufferDesc uboDesc;
	uboDesc.usage = SFE::Render::BufferUsage::Uniform;
	uboDesc.cpuAccess = SFE::Render::CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resX = static_cast<float>(w);
	mParams.resY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);

	SFE::Render::DescriptorSetLayoutDesc marchLayout;
	marchLayout.bindings.emplace_back(0, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	marchLayout.bindings.emplace_back(8, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	marchLayout.bindings.emplace_back(5, SFE::Render::DescriptorType::UniformBuffer, SFE::Render::ShaderStage::Fragment);
	marchLayout.bindings.emplace_back(11, SFE::Render::DescriptorType::UniformBuffer, SFE::Render::ShaderStage::Fragment);
	marchLayout.bindings.emplace_back(15, SFE::Render::DescriptorType::UniformBuffer, SFE::Render::ShaderStage::Fragment);
	mMarchLayout = rhi->createDescriptorSetLayout(marchLayout);
	mMarchSet = rhi->allocateDescriptorSet(mMarchLayout);

	SFE::Render::DescriptorSetLayoutDesc applyLayout;
	applyLayout.bindings.emplace_back(0, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	applyLayout.bindings.emplace_back(1, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	mApplyLayout = rhi->createDescriptorSetLayout(applyLayout);
	mApplySet = rhi->allocateDescriptorSet(mApplyLayout);

	mMarchPipeline = makeFs(rhi, mRenderPass, mMarchLayout, "shaders/deferred_shading.vs", "shaders/volumetric.fs");
	mApplyPipeline = makeFs(rhi, mRenderPass, mApplyLayout, "shaders/deferred_shading.vs", "shaders/volumetric_apply.fs");
}

void VolumetricPass::render(SystemsModule::RenderData& renderDataHandle, SFE::Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	auto& frame = renderDataHandle.frame;
	const auto src = frame.texture(SFE::Render::FrameId::Final);
	const auto pos = frame.texture(SFE::Render::FrameId::GBufferPosition);
	const auto shadow = frame.texture(SFE::Render::FrameId::ShadowCsm);
	if (!renderDataHandle.graphics.volumetric) {
		return;
	}
	if (!src || !pos || !mMarchPipeline || !mApplyPipeline) {
		return;
	}

	const auto* csm = frame.data<SFE::Render::CsmFrame>(SFE::Render::FrameId::Csm);
	mParams.viewX = renderDataHandle.mCameraPos.x;
	mParams.viewY = renderDataHandle.mCameraPos.y;
	mParams.viewZ = renderDataHandle.mCameraPos.z;
	mParams.density = renderDataHandle.graphics.volumetricDensity;
	mParams.anisotropy = renderDataHandle.graphics.volumetricAnisotropy;
	mParams.intensity = renderDataHandle.graphics.volumetricIntensity;
	mParams.maxDistance = renderDataHandle.graphics.volumetricDistance;
	if (csm) {
		mParams.lightX = csm->lightDirection.x;
		mParams.lightY = csm->lightDirection.y;
		mParams.lightZ = csm->lightDirection.z;
		mParams.colorX = csm->lightColor.x;
		mParams.colorY = csm->lightColor.y;
		mParams.colorZ = csm->lightColor.z;
		mParams.cascadeCount = static_cast<int>(csm->shadowCascades.size());
	}
	rhi->updateBuffer(mParamsUBO, mParams);

	std::vector<SFE::Render::WriteDescriptor> marchWrites;
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mMarchSet;
		w.binding = 0;
		w.type = SFE::Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ pos, frame.sampler(SFE::Render::FrameId::GBufferPosition) ? frame.sampler(SFE::Render::FrameId::GBufferPosition) : mNearest });
		marchWrites.push_back(std::move(w));
	}
	if (shadow) {
		SFE::Render::WriteDescriptor w{};
		w.set = mMarchSet;
		w.binding = 8;
		w.type = SFE::Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ shadow, frame.sampler(SFE::Render::FrameId::ShadowCsm) });
		marchWrites.push_back(std::move(w));
	}
	if (renderDataHandle.cameraMatricesUBO) {
		SFE::Render::WriteDescriptor w{};
		w.set = mMarchSet;
		w.binding = 5;
		w.type = SFE::Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		marchWrites.push_back(std::move(w));
	}
	if (const auto mats = frame.buffer(SFE::Render::FrameId::ShadowMatrices)) {
		SFE::Render::WriteDescriptor w{};
		w.set = mMarchSet;
		w.binding = 11;
		w.type = SFE::Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ mats });
		marchWrites.push_back(std::move(w));
	}
	{
		SFE::Render::WriteDescriptor w{};
		w.set = mMarchSet;
		w.binding = 15;
		w.type = SFE::Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ mParamsUBO });
		marchWrites.push_back(std::move(w));
	}
	rhi->updateDescriptorSets(marchWrites);

	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	{
		SFE::Render::RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mHalfFbo;
		beginInfo.renderPass = mRenderPass;
		beginInfo.width = std::max(1, screen.renderW / 2);
		beginInfo.height = std::max(1, screen.renderH / 2);
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mMarchPipeline);
		rhi->cmdBindDescriptorSets(cmd, mMarchPipeline, 0, &mMarchSet, 1);
		SFE::Render::Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}

	{
		std::vector<SFE::Render::WriteDescriptor> applyWrites;
		{
			SFE::Render::WriteDescriptor w{};
			w.set = mApplySet;
			w.binding = 0;
			w.type = SFE::Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ src, mLinear });
			applyWrites.push_back(std::move(w));
		}
		{
			SFE::Render::WriteDescriptor w{};
			w.set = mApplySet;
			w.binding = 1;
			w.type = SFE::Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mHalfTex, mLinear });
			applyWrites.push_back(std::move(w));
		}
		rhi->updateDescriptorSets(applyWrites);

		SFE::Render::RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mFbo;
		beginInfo.renderPass = mRenderPass;
		beginInfo.width = screen.renderW;
		beginInfo.height = screen.renderH;
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mApplyPipeline);
		rhi->cmdBindDescriptorSets(cmd, mApplyPipeline, 0, &mApplySet, 1);
		SFE::Render::Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}

	frame.set(SFE::Render::FrameId::Final, { .texture = mColor, .sampler = mLinear, .framebuffer = mFbo });
}
