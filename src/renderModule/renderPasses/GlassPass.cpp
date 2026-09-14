#include "GlassPass.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/Material.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "mathModule/VectorOperations.h"
#include "mathModule/MatrixOperators.h"
#include "multithreading/ThreadPool.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ChunksSystem.h"

using namespace SFE::Render::RenderPasses;

void GlassPass::init(SystemsModule::RenderData& renderDataHandle) {

	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	TextureDesc copyDesc;
	copyDesc.width = w;
	copyDesc.height = h;
	copyDesc.format = TextureFormat::RGBA16F;
	copyDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mSceneCopy = rhi->createTexture(copyDesc);

	SamplerDesc samplerDesc;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.magFilter = FilterMode::Linear;
	// Off-screen refraction must not ClampToEdge — that stretches the last
	// column/row into a visible frame. Mirror keeps real scene content.
	samplerDesc.addressU = AddressMode::MirrorRepeat;
	samplerDesc.addressV = AddressMode::MirrorRepeat;
	samplerDesc.addressW = AddressMode::MirrorRepeat;
	mSampler = rhi->createSampler(samplerDesc);

	FramebufferDesc copyFb;
	copyFb.width = w;
	copyFb.height = h;
	copyFb.colorAttachments = { { mSceneCopy } };
	mSceneCopyFbo = rhi->createFramebuffer(copyFb);

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
	rpDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	rpDesc.colorAttachments[0].format = TextureFormat::RGBA16F;
	rpDesc.hasDepthStencil = true;
	rpDesc.depthAttachment.loadOp = AttachmentLoadOp::Load;
	rpDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
	rpDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT24;
	mRenderPass = rhi->createRenderPass(rpDesc);

	BufferDesc uboDesc;
	uboDesc.usage = BufferUsage::Uniform;
	uboDesc.cpuAccess = CpuAccess::Write;
	uboDesc.size = sizeof(Params);
	mParams.resolutionX = static_cast<float>(w);
	mParams.resolutionY = static_cast<float>(h);
	mParamsUBO = rhi->createBuffer(uboDesc, &mParams);

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(6, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(13, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.push_back({ 16, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, static_cast<uint32_t>(GpuMaterialTable::kGlassTextureSlots) });
	layoutDesc.bindings.emplace_back(1, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(2, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(3, DescriptorType::StorageBuffer, ShaderStage::Vertex);
	layoutDesc.bindings.emplace_back(7, DescriptorType::StorageBuffer, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, DescriptorType::UniformBuffer, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	mSet = rhi->allocateDescriptorSet(mLayout);

	GraphicsPipelineDesc pipelineDesc;
	Render::VertexLayoutDesc& layout = pipelineDesc.layout;
	layout.bindings.push_back({ 0, sizeof(Vertex3D), Render::VertexInputRate::PerVertex });
	layout.attributes.push_back({ 0, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, position)), true });
	layout.attributes.push_back({ 1, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, normal)), true });
	layout.attributes.push_back({ 2, 0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords)), true });
	layout.attributes.push_back({ 3, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, tangent)), true });
	layout.attributes.push_back({ 4, 0, Render::VertexFormat::Float3, static_cast<std::uint32_t>(offsetof(Vertex3D, biTangent)), true });
	layout.attributes.push_back({ 5, 0, Render::VertexFormat::Int4, static_cast<std::uint32_t>(offsetof(Vertex3D, boneIDs)), false });
	layout.attributes.push_back({ 6, 0, Render::VertexFormat::Float4, static_cast<std::uint32_t>(offsetof(Vertex3D, weights)), false });
	layout.attributes.push_back({ 7, 0, Render::VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex3D, texCoords1)), true });
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.frontFace = FrontFace::CounterClockWise;
	pipelineDesc.depthStencilState.depthTest = true;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.depthStencilState.depthCompareOp = CompareOp::LessEqual;
	pipelineDesc.blend.enabled = true;
	pipelineDesc.blend.srcColor = BlendFactor::One;
	pipelineDesc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
	pipelineDesc.renderPass = mRenderPass;
	pipelineDesc.descriptorSetLayouts = { mLayout };
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/g_buffer.vs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/glass.fs");
	mPipeline = rhi->createGraphicsPipeline(pipelineDesc);
}

void GlassPass::ensureTargets(SystemsModule::RenderData& renderDataHandle) {
	auto& frame = renderDataHandle.frame;
	const auto finalTex = frame.texture(FrameId::Final);
	const auto depthTex = frame.texture(FrameId::GBufferDepth);
	if (!finalTex || !depthTex) {
		return;
	}
	if (mGlassFbo && mFinalTex.id == finalTex.id && mDepthTex.id == depthTex.id) {
		return;
	}

	auto* rhi = Engine::instance()->rhi();
	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;

	FramebufferDesc glassFb;
	glassFb.width = w;
	glassFb.height = h;
	glassFb.renderPass = mRenderPass;
	glassFb.colorAttachments = { { finalTex } };
	glassFb.depthStencilAttachment = { .texture = depthTex };
	mGlassFbo = rhi->createFramebuffer(glassFb);
	mFinalTex = finalTex;
	mDepthTex = depthTex;
}

void GlassPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	ensureTargets(renderDataHandle);
	const auto srcFbo = renderDataHandle.frame.framebuffer(FrameId::Final);
	if (!mGlassFbo || !srcFbo || !mPipeline) {
		return;
	}

	// Culled and sorted far to near on the GPU; outside the render pass, since it is compute.
	auto* renderSystem = ECSHandler::getSystem<SystemsModule::RenderSystem>();
	auto& scene = renderSystem->scene();
	const auto materials = renderSystem->materials().upload();
	if (!scene.sortGlass(cmd, mView, renderDataHandle.mCamFrustum, renderDataHandle.mCameraPos, renderDataHandle.mViewDir)) {
		return;
	}

	auto& frame = renderDataHandle.frame;

	mParams.viewPosX = renderDataHandle.mCameraPos.x;
	mParams.viewPosY = renderDataHandle.mCameraPos.y;
	mParams.viewPosZ = renderDataHandle.mCameraPos.z;
	mParams.roughness = renderDataHandle.graphics.glassRoughness;
	mParams.sphereSpacing = 8.f;
	if (auto* chunks = ECSHandler::getSystem<SystemsModule::ChunksSystem>()) {
		if (const auto* dd = static_cast<const SystemsModule::ChunksDebugData*>(chunks->getDebugData())) {
			mParams.sphereSpacing = dd->sphereSpacing;
		}
	}
	if (const auto* csm = frame.data<CsmFrame>(FrameId::Csm)) {
		mParams.lightX = csm->lightDirection.x;
		mParams.lightY = csm->lightDirection.y;
		mParams.lightZ = csm->lightDirection.z;
		mParams.colorX = csm->lightColor.x;
		mParams.colorY = csm->lightColor.y;
		mParams.colorZ = csm->lightColor.z;
	}

	std::vector<WriteDescriptor> writes;
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 6;
		w.images.push_back({ mSceneCopy, mSampler });
		writes.push_back(std::move(w));
	}
	{
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 15;
		w.buffers.push_back({ mParamsUBO });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblIrradiance)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 12;
		w.images.push_back({ frame.texture(FrameId::IblIrradiance), frame.sampler(FrameId::IblIrradiance) });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblPrefilter)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 13;
		w.images.push_back({ frame.texture(FrameId::IblPrefilter), frame.sampler(FrameId::IblPrefilter) });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.cameraMatricesUBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 5;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.transformsSSBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 1;
		w.buffers.push_back({ renderDataHandle.transformsSSBO });
		writes.push_back(std::move(w));
	}
	if (renderDataHandle.bonesSSBO) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 2;
		w.buffers.push_back({ renderDataHandle.bonesSSBO });
		writes.push_back(std::move(w));
	}
	if (frame.texture(FrameId::IblBrdf)) {
		WriteDescriptor w{};
		w.set = mSet;
		w.binding = 14;
		w.images.push_back({ frame.texture(FrameId::IblBrdf), frame.sampler(FrameId::IblBrdf) });
		writes.push_back(std::move(w));
	}
	// The glass albedo textures as one array, so every glass instance draws from the same set.
	{
		const auto* white = AssetsModule::TextureHandler::loadTexture("white.png");
		const auto textures = renderSystem->materials().glassTextures();
		WriteDescriptor albedo{};
		albedo.set = mSet;
		albedo.binding = 16;
		for (size_t i = 0; i < GpuMaterialTable::kGlassTextureSlots; ++i) {
			const auto tex = i < textures.size() && textures[i] ? textures[i] : white->texture;
			albedo.images.push_back({ tex, white->sampler });
		}
		writes.push_back(std::move(albedo));
	}
	writes.push_back({ .set = mSet, .binding = 3, .buffers{ { mView.visible } } });
	writes.push_back({ .set = mSet, .binding = 7, .buffers{ { materials } } });
	rhi->updateDescriptorSets(writes);

	// Per-material ior, opacity and tint come from the material table; the rest is the same for every draw.
	rhi->cmdUpdateBuffer(cmd, mParamsUBO, mParams);

	const int w = Engine::instance()->getWindow()->getScreenData().renderW;
	const int h = Engine::instance()->getWindow()->getScreenData().renderH;
	RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mGlassFbo;
	beginInfo.renderPass = mRenderPass;
	beginInfo.width = w;
	beginInfo.height = h;

	rhi->cmdBlit(cmd, srcFbo, mSceneCopyFbo, BlitMask::Color, BlitFilter::Nearest);
	rhi->cmdBeginRenderPass(cmd, beginInfo);
	const auto* meshes = SFE::MeshVaoRegistry::instance();
	rhi->cmdBindPipeline(cmd, mPipeline);
	rhi->cmdBindVertexBuffer(cmd, 0, meshes->vertexBuffer(), 0);
	rhi->cmdBindIndexBuffer(cmd, meshes->indexBuffer(), IndexType::Uint32, 0);
	rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mSet, 1);
	scene.drawSorted(cmd, mView);
	rhi->cmdEndRenderPass(cmd);
}
