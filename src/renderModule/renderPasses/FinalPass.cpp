#include "FinalPass.h"

#include <algorithm>
#include <cstdio>

#include "assetsModule/shaderModule/ShaderBase.h"
#include "assetsModule/modelModule/Vertex.h"
#include "core/Engine.h"
#include "rhi/GraphicsBackend.h"
#include "renderModule/TextRenderer.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

SFE::Render::PipelineHandle FinalPass::makeFullscreen(SFE::Render::RenderHardwareInterface* rhi, RenderPassHandle renderPass, const char* vsPath, const char* fsPath) {
	GraphicsPipelineDesc pipelineDesc;
	pipelineDesc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	pipelineDesc.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	pipelineDesc.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.descriptorSetLayouts = { mLayout };
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, vsPath);
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, fsPath);
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

	DescriptorSetLayoutDesc layoutDesc;
	layoutDesc.bindings.emplace_back(0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
	layoutDesc.bindings.emplace_back(15, DescriptorType::UniformBufferDynamic, ShaderStage::Fragment);
	mLayout = rhi->createDescriptorSetLayout(layoutDesc);
	// One set per frame slot: what it binds alternates with the slot, and writing a set the previous
	// frame still binds costs a copy of it.
	for (uint32_t i = 0; i < (std::max)(1u, rhi->framesInFlight()); ++i) {
		mBloomSets.push_back(rhi->allocateDescriptorSet(mLayout));
		mPostSets.push_back(rhi->allocateDescriptorSet(mLayout));
	}

	RenderPassDesc bloomRp;
	bloomRp.colorAttachmentCount = 1;
	bloomRp.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	bloomRp.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	bloomRp.colorAttachments[0].format = TextureFormat::RGBA16F;
	mBloomPass = rhi->createRenderPass(bloomRp);

	RenderPassDesc displayRp;
	displayRp.colorAttachmentCount = 1;
	displayRp.colorAttachments[0].loadOp = AttachmentLoadOp::Clear;
	displayRp.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
	displayRp.colorAttachments[0].format = TextureFormat::RGBA8;
	mDisplayPass = rhi->createRenderPass(displayRp);

	mBloomPipeline = makeFullscreen(rhi, mBloomPass, "shaders/deferred_shading.vs", "shaders/bloom_extract.fs");
	mPostPipeline = makeFullscreen(rhi, mDisplayPass, "shaders/deferred_shading.vs", "shaders/post.fs");

	TextureDesc bloomDesc;
	bloomDesc.width = std::max(1, w / 4);
	bloomDesc.height = std::max(1, h / 4);
	bloomDesc.format = TextureFormat::RGBA16F;
	bloomDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mBloomTex = rhi->createTexture(bloomDesc);
	FramebufferDesc bloomFb;
	bloomFb.width = bloomDesc.width;
	bloomFb.height = bloomDesc.height;
	bloomFb.renderPass = mBloomPass;
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
	displayFb.renderPass = mDisplayPass;
	displayFb.colorAttachments = { { mDisplayTex } };
	mDisplayFbo = rhi->createFramebuffer(displayFb);

	TextRenderer::instance()->bindRenderPass(mDisplayPass);
	mOverlayFont = FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 22);

	GraphicsPipelineDesc shapeDesc;
	shapeDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/overlay_shape.vs");
	shapeDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/overlay_shape.fs");
	shapeDesc.layout.bindings.push_back({ 0, static_cast<uint32_t>(sizeof(Math::Vec2)), VertexInputRate::PerVertex });
	shapeDesc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, 0, false });
	shapeDesc.topology = PrimitiveTopology::Triangles;
	shapeDesc.cullMode = CullMode::None;
	shapeDesc.depthStencilState.depthTest = false;
	shapeDesc.depthStencilState.depthWrite = false;
	shapeDesc.blend.enabled = true;
	shapeDesc.blend.srcColor = BlendFactor::SrcAlpha;
	shapeDesc.blend.dstColor = BlendFactor::OneMinusSrcAlpha;
	shapeDesc.renderPass = mDisplayPass;
	shapeDesc.pushConstantSize = static_cast<uint32_t>(sizeof(Math::Vec4));
	shapeDesc.pushConstantStages = ShaderStage::Fragment;
	mOverlayFillPipeline = rhi->createGraphicsPipeline(shapeDesc);
	shapeDesc.topology = PrimitiveTopology::Lines;
	mOverlayLinePipeline = rhi->createGraphicsPipeline(shapeDesc);
}


namespace {
	// The overlay panel, in pixels from the top left.
	constexpr float kPanelX = 12.f;
	constexpr float kPanelY = 12.f;
	constexpr float kPanelWidth = 380.f;
	constexpr float kTextHeight = 86.f;
	constexpr float kGraphHeight = 44.f;
	constexpr float kGraphGap = 6.f;

	const SFE::Math::Vec4 kPanelColor{ 0.f, 0.f, 0.f, 0.45f };
	const SFE::Math::Vec4 kFrameColor{ 1.f, 0.92f, 0.2f, 1.f };
	const SFE::Math::Vec4 kGpuColor{ 0.45f, 0.9f, 0.55f, 1.f };
}

void FinalPass::addRect(const Rect& rect, std::vector<Math::Vec2>& out) const {
	const auto toClip = [this](float x, float y) {
		return Math::Vec2{ x / mScreenWidth * 2.f - 1.f, 1.f - y / mScreenHeight * 2.f };
	};
	const auto topLeft = toClip(rect.x, rect.y);
	const auto bottomRight = toClip(rect.x + rect.width, rect.y + rect.height);
	out.push_back(topLeft);
	out.push_back({ bottomRight.x, topLeft.y });
	out.push_back(bottomRight);
	out.push_back(topLeft);
	out.push_back(bottomRight);
	out.push_back({ topLeft.x, bottomRight.y });
}

void FinalPass::addGraph(const Rect& rect, const float* samples, const int count, const int cursor, const float scale,
	std::vector<Math::Vec2>& out) const {
	if (count < 2 || scale <= 0.f) {
		return;
	}
	const auto point = [&](int i) {
		// The cursor holds the oldest sample, so time runs left to right from there.
		const float value = samples[(cursor + i) % count];
		const float x = rect.x + rect.width * static_cast<float>(i) / static_cast<float>(count - 1);
		const float y = rect.y + rect.height * (1.f - (std::min)(value, scale) / scale);
		return Math::Vec2{ x / mScreenWidth * 2.f - 1.f, 1.f - y / mScreenHeight * 2.f };
	};
	auto previous = point(0);
	for (int i = 1; i < count; ++i) {
		const auto current = point(i);
		out.push_back(previous);
		out.push_back(current);
		previous = current;
	}
}

void FinalPass::drawShapes(const Render::CommandListHandle cmd, const PipelineHandle pipeline,
	const std::vector<Math::Vec2>& vertices, const Math::Vec4& color) {
	if (vertices.empty() || !pipeline) {
		return;
	}
	auto* rhi = Engine::instance()->rhi();
	const auto allocation = rhi->allocTransient(vertices.size() * sizeof(Math::Vec2));
	if (!allocation) {
		return;
	}
	std::memcpy(allocation.data, vertices.data(), vertices.size() * sizeof(Math::Vec2));
	rhi->cmdBindPipeline(cmd, pipeline);
	rhi->cmdBindVertexBuffer(cmd, 0, allocation.buffer, allocation.offset);
	rhi->cmdPushConstants(cmd, pipeline, ShaderStage::Fragment, &color, sizeof(color));
	rhi->cmdDraw(cmd, static_cast<uint32_t>(vertices.size()));
}

// Which backend and GPU the frame ran on, what it cost, and the same over the last few seconds.
void FinalPass::drawOverlay(const Render::CommandListHandle cmd, const int screenWidth, const int screenHeight) {
	auto* rhi = Engine::instance()->rhi();
	const auto* info = SystemsModule::RenderDebugInfo::instance();
	mScreenWidth = static_cast<float>(screenWidth);
	mScreenHeight = static_cast<float>(screenHeight);
	if (mScreenWidth <= 0.f || mScreenHeight <= 0.f) {
		return;
	}

	float worst = 0.f;
	float sum = 0.f;
	int samples = 0;
	for (const auto ms : info->frameMsHistory) {
		if (ms <= 0.f) {
			continue; // the ring has not filled yet
		}
		worst = (std::max)(worst, ms);
		sum += ms;
		++samples;
	}
	const float average = samples ? sum / static_cast<float>(samples) : 0.f;
	// One stall would flatten every other frame against the bottom, so the scale follows the average and lets
	// a spike past four times it run off the top; the text still says how bad the worst frame was.
	const float scale = (std::max)(8.f, (std::min)(worst, average * 4.f)) * 1.2f;

	const Rect frameGraph{ kPanelX + 8.f, kPanelY + kTextHeight, kPanelWidth - 16.f, kGraphHeight };
	const Rect gpuGraph{ frameGraph.x, frameGraph.y + kGraphHeight + kGraphGap, frameGraph.width, kGraphHeight };

	mOverlayVertices.clear();
	addRect({ kPanelX, kPanelY, kPanelWidth, gpuGraph.y + gpuGraph.height + 8.f - kPanelY }, mOverlayVertices);
	drawShapes(cmd, mOverlayFillPipeline, mOverlayVertices, kPanelColor);

	mOverlayVertices.clear();
	addGraph(frameGraph, info->frameMsHistory, SystemsModule::RenderDebugInfo::kHistory, info->historyCursor, scale, mOverlayVertices);
	drawShapes(cmd, mOverlayLinePipeline, mOverlayVertices, kFrameColor);

	mOverlayVertices.clear();
	addGraph(gpuGraph, info->gpuMsHistory, SystemsModule::RenderDebugInfo::kHistory, info->historyCursor, scale, mOverlayVertices);
	drawShapes(cmd, mOverlayLinePipeline, mOverlayVertices, kGpuColor);

	char overlay[256];
	std::snprintf(
		overlay,
		sizeof(overlay),
		"%s | %s\nframe %.1f ms (%.0f fps)  worst %.1f\ngpu %.1f ms  cpu %.1f ms",
		graphicsBackendName(activeGraphicsBackend()),
		rhi->deviceName(),
		static_cast<double>(average),
		average > 0.f ? 1000.0 / static_cast<double>(average) : 0.0,
		static_cast<double>(worst),
		static_cast<double>(info->gpuMs),
		static_cast<double>(info->cpuMs)
	);
	TextRenderer::instance()->renderText(cmd, overlay, kPanelX + 8.f, kPanelY + 22.f, 1.f, Math::Vec3{ 1.f, 0.92f, 0.2f }, mOverlayFont);
}

void FinalPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	const auto src = renderDataHandle.frame.texture(FrameId::Final);
		if (!src || !mPostPipeline) {
			return;
		}

	auto* rhi = Engine::instance()->rhi();
	const auto mBloomSet = mBloomSets[rhi->frameInFlight() % mBloomSets.size()];
	const auto mPostSet = mPostSets[rhi->frameInFlight() % mPostSets.size()];
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	auto& gfx = renderDataHandle.graphics;

	mPostParams.bloomStrength = gfx.bloom ? gfx.bloomStrength : 0.f;
	mPostParams.exposure = gfx.exposure;
	mPostParams.vignette = gfx.vignette;
	mPostParams.ca = gfx.chromaticAberration;

	// A debug term carries a value to be read off the image, not a picture to be graded. Exposure
	// and vignette silently rescale it, which turns any absolute reading into nonsense.
	if (Engine::instance()->lightingDebugTerm() != 0) {
		mPostParams.bloomStrength = 0.f;
		mPostParams.exposure = 1.f;
		mPostParams.vignette = 0.f;
		mPostParams.ca = 0.f;
	}
	const auto post = rhi->allocTransient(mPostParams);

	if (gfx.bloom && mBloomPipeline) {
		WriteDescriptor w{};
		w.set = mBloomSet;
		w.binding = 0;
		w.images.push_back({ src, mSampler });
		// The layout's dynamic binding 15 must hold a buffer, and Vulkan wants its offset at the bind.
		WriteDescriptor params{};
		params.set = mBloomSet;
		params.binding = 15;
		params.buffers.push_back({ post.buffer, 0, sizeof(PostParams) });
		rhi->updateDescriptorSets({ w, params });

		RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mBloomFbo;
		beginInfo.renderPass = mBloomPass;
		beginInfo.width = std::max(1, screen.renderW / 4);
		beginInfo.height = std::max(1, screen.renderH / 4);
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mBloomPipeline);
		rhi->cmdBindDescriptorSets(cmd, mBloomPipeline, 0, &mBloomSet, 1, &post.offset, 1);
		Utils::renderQuad(cmd);
		rhi->cmdEndRenderPass(cmd);
	}

	{
		std::vector<WriteDescriptor> writes;
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 0;
			w.images.push_back({ src, mSampler });
			writes.push_back(std::move(w));
		}
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 1;
			w.images.push_back({ mBloomTex, mSampler });
			writes.push_back(std::move(w));
		}
		{
			WriteDescriptor w{};
			w.set = mPostSet;
			w.binding = 15;
			w.buffers.push_back({ post.buffer, 0, sizeof(PostParams) });
			writes.push_back(std::move(w));
		}
		rhi->updateDescriptorSets(writes);

		RenderPassBeginInfo beginInfo;
		beginInfo.framebuffer = mDisplayFbo;
		beginInfo.renderPass = mDisplayPass;
		beginInfo.width = screen.renderW;
		beginInfo.height = screen.renderH;
		rhi->cmdBeginRenderPass(cmd, beginInfo);
		rhi->cmdBindPipeline(cmd, mPostPipeline);
		rhi->cmdBindDescriptorSets(cmd, mPostPipeline, 0, &mPostSet, 1, &post.offset, 1);
		Utils::renderQuad(cmd);
		if (mOverlayFont && !Engine::instance()->capturingFrame()) {
			drawOverlay(cmd, screen.renderW, screen.renderH);
		}
		rhi->cmdEndRenderPass(cmd);
	}

	renderDataHandle.frame.set(FrameId::Final, { .texture = mDisplayTex, .sampler = mSampler, .framebuffer = mDisplayFbo });
	rhi->setPresentBlitSource(mDisplayTex, mSampler);
}
