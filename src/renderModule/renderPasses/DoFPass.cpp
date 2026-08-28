#include "DoFPass.h"

#include "assetsModule/modelModule/Vertex.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

void DoFPass::init(SystemsModule::RenderData&) {
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

	SFE::Render::GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(SFE::Vertex2D)), SFE::Render::VertexInputRate::PerVertex);
	pipelineDesc.layout.attributes.emplace_back(0, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, position)), false);
	pipelineDesc.layout.attributes.emplace_back(1, 0, SFE::Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(SFE::Vertex2D, texCoords)), false);
	pipelineDesc.topology = SFE::Render::PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = SFE::Render::CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.shaders.vertex = rhi->createShaderModule({ SFE::Render::ShaderStage::Vertex, "dof vs", SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/deferred_shading.vs") });
	pipelineDesc.shaders.fragment = rhi->createShaderModule({ SFE::Render::ShaderStage::Fragment, "dof fs", SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/dof.fs") });
	mPipeline = rhi->createGraphicsPipeline(pipelineDesc);

	SFE::Render::RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = SFE::Render::AttachmentLoadOp::Clear;
	rpDesc.colorAttachments[0].storeOp = SFE::Render::AttachmentStoreOp::Store;
	mRenderPass = rhi->createRenderPass(rpDesc);

	SFE::Render::TextureDesc colorDesc;
	colorDesc.width = w;
	colorDesc.height = h;
	colorDesc.format = SFE::Render::TextureFormat::RGBA16F;
	colorDesc.usage = SFE::Render::TextureUsage::ColorAttachment | SFE::Render::TextureUsage::Sampled;
	mColor = rhi->createTexture(colorDesc);
	SFE::Render::FramebufferDesc fb;
	fb.width = w;
	fb.height = h;
	fb.colorAttachments = { { mColor } };
	mFbo = rhi->createFramebuffer(fb);

	SFE::Render::BufferDesc uboDesc;
	uboDesc.usage = SFE::Render::BufferUsage::Uniform;
	uboDesc.cpuAccess = SFE::Render::CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resX = static_cast<float>(w);
	mParams.resY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);

	SFE::Render::DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, SFE::Render::DescriptorType::CombinedImageSampler, SFE::Render::ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, SFE::Render::DescriptorType::UniformBuffer, SFE::Render::ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mLayout);
}

void DoFPass::render(SystemsModule::RenderData& renderDataHandle, SFE::Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	auto& frame = renderDataHandle.frame;
	const auto src = frame.texture(SFE::Render::FrameId::Final);
	const auto pos = frame.texture(SFE::Render::FrameId::GBufferPosition);
	if (!renderDataHandle.graphics.dof) {
		return;
	}
	if (!src || !pos || !mPipeline) {
		return;
	}

	mParams.viewX = renderDataHandle.mCameraPos.x;
	mParams.viewY = renderDataHandle.mCameraPos.y;
	mParams.viewZ = renderDataHandle.mCameraPos.z;
	mParams.focusDistance = renderDataHandle.graphics.dofFocus;
	mParams.focusRange = renderDataHandle.graphics.dofRange;
	mParams.maxBlur = renderDataHandle.graphics.dofBlur;
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	mParams.resX = screen.renderW > 0 ? static_cast<float>(screen.renderW) : 1.f;
	mParams.resY = screen.renderH > 0 ? static_cast<float>(screen.renderH) : 1.f;
	rhi->updateBuffer(mParamsUBO, mParams);

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

	SFE::Render::RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mFbo;
	beginInfo.renderPass = mRenderPass;
	beginInfo.width = screen.renderW;
	beginInfo.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, mPipeline);
	rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mSet, 1);
	SFE::Render::Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	frame.set(SFE::Render::FrameId::Final, { .texture = mColor, .sampler = mLinear, .framebuffer = mFbo });
}
