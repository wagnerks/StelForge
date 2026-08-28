#include "SSRPass.h"

#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/Engine.h"
#include "renderModule/Utils.h"
#include "debugModule/Benchmark.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

void SSRPass::init(SystemsModule::RenderData& renderDataHandle) {
	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	TextureDesc colorDesc;
	colorDesc.width = w;
	colorDesc.height = h;
	colorDesc.format = TextureFormat::RGBA16F;
	colorDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mColor = rhi->createTexture(colorDesc);

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	mSampler = rhi->createSampler(samplerDesc);

	FramebufferDesc fbDesc;
	fbDesc.width = w;
	fbDesc.height = h;
	fbDesc.colorAttachments = { { mColor } };
	mFbo = rhi->createFramebuffer(fbDesc);

	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	pipelineDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	pipelineDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, "ssr vs", SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/deferred_shading.vs") });
	pipelineDesc.shaders.fragment = rhi->createShaderModule({ ShaderStage::Fragment, "ssr fs", SFE::ShaderModule::ShaderBase::loadShaderCode("shaders/ssr.fs") });
	mPipeline = rhi->createGraphicsPipeline(pipelineDesc);

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	rpDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	mRenderPass = rhi->createRenderPass(rpDesc);

	BufferDesc uboDesc;
	uboDesc.usage = BufferUsage::Uniform;
	uboDesc.cpuAccess = CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resolutionX = static_cast<float>(w);
	mParams.resolutionY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(4, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(3, DescriptorType::UniformBuffer, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mLayout);

	std::vector<WriteDescriptor> initWrites;
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 3;
		w.type = DescriptorType::UniformBuffer;
		w.buffers.push_back({ mParamsUBO });
		initWrites.push_back(std::move(w));
	}
	rhi->updateDescriptorSets(initWrites);

	renderDataHandle.frame.set(FrameId::Final, { .texture = mColor, .sampler = mSampler, .framebuffer = mFbo });
}

void SSRPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	auto& frame = renderDataHandle.frame;
	const auto lit = frame.texture(FrameId::Lit);
	if (!renderDataHandle.graphics.ssr) {
		return;
	}
	if (!lit) {
		return;
	}

	mParams.viewPosX = renderDataHandle.mCameraPos.x;
	mParams.viewPosY = renderDataHandle.mCameraPos.y;
	mParams.viewPosZ = renderDataHandle.mCameraPos.z;
	mParams.maxDistance = renderDataHandle.graphics.ssrDistance;
	{
		const auto& screen = Engine::instance()->getWindow()->getScreenData();
		mParams.resolutionX = screen.renderW > 0 ? static_cast<float>(screen.renderW) : 1.f;
		mParams.resolutionY = screen.renderH > 0 ? static_cast<float>(screen.renderH) : 1.f;
	}
	rhi->updateBuffer(mParamsUBO, mParams);

	std::vector<WriteDescriptor> writes;
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 0;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::GBufferPosition), frame.sampler(FrameId::GBufferPosition) });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 1;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::GBufferNormal), frame.sampler(FrameId::GBufferNormal) });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 2;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ frame.texture(FrameId::GBufferAlbedo), frame.sampler(FrameId::GBufferAlbedo) });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 4;
		w.type = DescriptorType::CombinedImageSampler;
		w.images.push_back({ lit, mSampler });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 3;
		w.type = DescriptorType::UniformBuffer;
		w.buffers.push_back({ mParamsUBO });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.cameraMatricesUBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 5;
		w.type = DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		writes.push_back(std::move(w));
	}
	rhi->updateDescriptorSets(writes);

	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;
	RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mFbo;
	beginInfo.renderPass = mRenderPass;
	beginInfo.width = w;
	beginInfo.height = h;

	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, mPipeline);
	rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mSet, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	frame.set(FrameId::Final, { .texture = mColor, .sampler = mSampler, .framebuffer = mFbo });
}
