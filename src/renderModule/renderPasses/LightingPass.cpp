#include "LightingPass.h"
#include <algorithm>
#include <cmath>
#include "assetsModule/TextureHandler.h"
#include "renderModule/IblBaker.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"
#include "imgui.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "logsModule/logger.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;


LightingPass::LightingPass() {

}

void LightingPass::init(SystemsModule::RenderData& renderDataHandle)
{
	RenderPass::init(renderDataHandle);
	
	auto* rhi = Engine::instance()->rhi();

	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(6, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(7, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(8, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(12, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(13, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(14, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);

	mTexSetLayout = rhi->createDescriptorSetLayout(texLayoutDesc);
	mTexSet = rhi->allocateDescriptorSet(mTexSetLayout);

	Render::DescriptorSetLayoutDesc uboLayoutDesc;
	uboLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	uboLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	uboLayoutDesc.bindings.emplace_back(9, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	uboLayoutDesc.bindings.emplace_back(10, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	uboLayoutDesc.bindings.emplace_back(11, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	uboLayoutDesc.bindings.emplace_back(15, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	mUboSetLayout = rhi->createDescriptorSetLayout(uboLayoutDesc);
	mUboSet = rhi->allocateDescriptorSet(mUboSetLayout);

	mTexSetInitialized = true;

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0] = {
		.loadOp = AttachmentLoadOp::Clear,
		.storeOp = AttachmentStoreOp::Store,
		.clearColor = Math::Vec4{0.f, 0.f, 0.f, 1.f},
		.format = TextureFormat::RGBA16F,
	};

	renderPass = rhi->createRenderPass(rpDesc);

	FramebufferDesc fbDesc;
	fbDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	fbDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	fbDesc.renderPass = renderPass;

	TextureDesc litDesc;
	litDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	litDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	litDesc.format = TextureFormat::RGBA16F;
	litDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mPresentTex[0] = rhi->createTexture(litDesc);
	mPresentTex[1] = rhi->createTexture(litDesc);

	for (int i = 0; i < 2; ++i) {
		FramebufferDesc presentFb = fbDesc;
		presentFb.colorAttachments = { { mPresentTex[i] } };
		mPresentFbo[i] = rhi->createFramebuffer(presentFb);
	}
	{
		SamplerDesc presentSamp;
		presentSamp.minFilter = FilterMode::Linear;
		presentSamp.magFilter = FilterMode::Linear;
		mPresentSampler = rhi->createSampler(presentSamp);
	}
	renderDataHandle.frame.set(FrameId::Lit, { .texture = mPresentTex[0], .sampler = mPresentSampler });
	renderDataHandle.frame.set(FrameId::Final, { .texture = mPresentTex[0], .sampler = mPresentSampler, .framebuffer = mPresentFbo[0] });

	GraphicsPipelineDesc pipelineDesc{};
	Render::VertexLayoutDesc& layout = pipelineDesc.layout;
	layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);

	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.descriptorSetLayouts = { mTexSetLayout, mUboSetLayout };
	pipelineDesc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/deferred_shading.vs");
	pipelineDesc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/deferred_shading.fs");
	pipeline = rhi->createGraphicsPipeline(pipelineDesc);

	BufferDesc plubodesc;
	plubodesc.usage = BufferUsage::Uniform;
	plubodesc.cpuAccess = CpuAccess::Write;
	plubodesc.size = sizeof(pointLight); // max 12 lights

	pointLightsUBO = rhi->createBuffer(plubodesc, pointLight);

	BufferDesc shadowsUBODesc;
	shadowsUBODesc.usage = BufferUsage::Uniform;
	shadowsUBODesc.cpuAccess = CpuAccess::Write;
	shadowsUBODesc.size = sizeof(CascadedShadowBlock);
	cascadedShadowsUBO = rhi->createBuffer(shadowsUBODesc, &cascadedShadowData);

	BufferDesc pcDesc;
	pcDesc.usage = BufferUsage::Uniform;
	pcDesc.cpuAccess = CpuAccess::Write;
	pcDesc.size = sizeof(DrawData);
	pushConstantsUBO = rhi->createBuffer(pcDesc);

	BufferDesc lmDesc;
	lmDesc.usage = BufferUsage::Uniform;
	lmDesc.cpuAccess = CpuAccess::Write;
	lmDesc.size = sizeof(Math::Mat4) * 6;
	lmDesc.debugName = "LightingPass_LightSpaceMatrices";
	lightMatricesUBO = rhi->createBuffer(lmDesc);

	std::vector<Render::WriteDescriptor> writes = {
		{ .set = mUboSet, .binding = 10, .buffers{ {pointLightsUBO} } },
		{ .set = mUboSet, .binding = 9, .buffers{ {cascadedShadowsUBO} } },
		{ .set = mUboSet, .binding = 15, .buffers{ {pushConstantsUBO} } },
		{ .set = mUboSet, .binding = 11, .buffers{ {lightMatricesUBO} } },
	};

	rhi->updateDescriptorSets(writes);

	{
		TextureDesc dummyShadow;
		dummyShadow.width = 1;
		dummyShadow.height = 1;
		dummyShadow.depth = 6;
		dummyShadow.dimension = TextureDimension::Array;
		dummyShadow.format = TextureFormat::DEPTH_COMPONENT32;
		dummyShadow.usage = TextureUsage::Sampled;
		mDummyPointShadow = rhi->createTexture(dummyShadow);
		SamplerDesc dummySamp;
		dummySamp.minFilter = FilterMode::Nearest;
		dummySamp.magFilter = FilterMode::Nearest;
		dummySamp.addressU = AddressMode::ClampToEdge;
		dummySamp.addressV = AddressMode::ClampToEdge;
		dummySamp.addressW = AddressMode::ClampToEdge;
		dummySamp.compareOp = CompareOp::LessEqual;
		mDummyPointShadowSampler = rhi->createSampler(dummySamp);
		rhi->updateDescriptorSets({
			{ mTexSet, 7, {}, {{ mDummyPointShadow, mDummyPointShadowSampler }} },
			{ mUboSet, 3, {{ lightMatricesUBO }} },
		});
		mBoundPointShadow = { mDummyPointShadow, mDummyPointShadowSampler };
		mBoundPointMat = lightMatricesUBO;
	}

	mIbl = {};
	mIbl = SFE::Render::bakeIbl(rhi);
	if (mIbl.irradiance && mIbl.prefilter && mIbl.brdfLut) {
		std::vector<Render::WriteDescriptor> iblWrites{ {
			.set = mTexSet,
			.binding = 12,
			
			.images{ { mIbl.irradiance, mIbl.irradianceSampler } }
		},
		{
			.set = mTexSet,
			.binding = 13,
			
			.images{ { mIbl.prefilter, mIbl.prefilterSampler } }
		},
		{
			.set = mTexSet,
			.binding = 14,
			
			.images{ { mIbl.brdfLut, mIbl.lutSampler } }
		} };
		rhi->updateDescriptorSets(iblWrites);
		renderDataHandle.frame.set(FrameId::IblIrradiance, { .texture = mIbl.irradiance, .sampler = mIbl.irradianceSampler });
		renderDataHandle.frame.set(FrameId::IblPrefilter, { .texture = mIbl.prefilter, .sampler = mIbl.prefilterSampler });
		renderDataHandle.frame.set(FrameId::IblBrdf, { .texture = mIbl.brdfLut, .sampler = mIbl.lutSampler });
	}
}

void LightingPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	assert(mTexSetInitialized);
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	auto& frame = renderDataHandle.frame;
	const auto* point = frame.data<PointShadowFrame>(FrameId::PointShadow);
	const auto* csm = frame.data<CsmFrame>(FrameId::Csm);
	int offsetSum = 0;
	drawData.pointLightsSize = 0;
	if (point) {
		const size_t lightCount = (std::min)(point->shadowEntities.size(), static_cast<size_t>(12));
		for (size_t i = 0; i < lightCount; i++) {
			auto tc = ECSHandler::registry().pinComponent<const TransformComponent>(point->shadowEntities[i]);
			auto lightComp = ECSHandler::registry().pinComponent<const LightSourceComponent>(point->shadowEntities[i]);
			if (!tc || !lightComp) {
				continue;
			}

			auto& light = pointLight[i];

			light.Position = tc->getPos(true);
			light.texelSize = Math::Vec2{ lightComp->getTexelSize().x, lightComp->getTexelSize().y };
			light.bias = lightComp->getBias();
			light.samples = (std::clamp)(lightComp->getSamples(), 1, 64);
			light.radius = lightComp->mRadius;
			light.offset = offsetSum;

			light.Color = lightComp->getLightColor() * lightComp->getIntensity();
			light.Linear = lightComp->mLinear;
			light.Quadratic = lightComp->mQuadratic;
			if (lightComp->getType() == ComponentsModule::eLightType::POINT) {
				light.Type = 0;
				light.Layers = 6;
			}
			else {
				light.Type = 1;
				light.Layers = 1;
			}

			offsetSum += lightComp->getTypeOffset(lightComp->getType());
		}

		rhi->updateBuffer(pointLightsUBO, pointLight);
		drawData.pointLightsSize = static_cast<int>(lightCount);
	}

	if (csm && !csm->shadowCascadeLevels.empty()) {
		const auto& lightColor = csm->lightColor;
		drawData.ambientX = lightColor.x;
		drawData.ambientY = lightColor.y;
		drawData.ambientZ = lightColor.z;
		drawData.cascadeCount = static_cast<int>(csm->shadowCascades.size());
		drawData.shadowIntensity = csm->shadowsIntensity;

		cascadedShadowData.direction_intensity.xyz = csm->lightDirection;
		cascadedShadowData.direction_intensity.w = csm->shadowsIntensity;
		cascadedShadowData.color_and_misc.xyz = csm->lightColor;
		cascadedShadowData.color_and_misc.w = static_cast<float>(drawData.cascadeCount);
	
		for (size_t i = 0; i < csm->shadowCascades.size(); ++i) {
			auto& cascade = csm->shadowCascades[i];
			auto& cascadeRenderData = cascadedShadowData.data[i];

			const float invRes = 1.f / (std::max)(1.f, csm->resolution.x);
			cascadeRenderData.cascade.x = invRes;
			// y carried max(texelSize.x, invRes), but texelSize is never filled for cascades and no
			// shader read the slot -- it held a duplicate of x. It now carries the world size of one
			// shadow-map texel, which is what the normal-offset bias needs.
			cascadeRenderData.cascade.y = cascade.worldPerTexel;
			// bias is authored in depth units. That means very different world distances per
			// cascade (worldPerTexel and depthRange differ by ~40x between the near and far one),
			// which is worth revisiting -- but converting it to texel-relative units changes the
			// picture on both backends, so it is not a standalone fix.
			cascadeRenderData.cascade.z = std::isfinite(cascade.bias) ? cascade.bias : 0.f;
			cascadeRenderData.cascade.w = static_cast<float>(cascade.samples);
			cascadeRenderData.cascadeExtra.x = cascade.depthRange;
		}

		const auto& levels = csm->shadowCascadeLevels;
		auto splitAt = [&levels](size_t i) {
			return (i < levels.size()) ? levels[i] : 1.0e6f;
		};
		cascadedShadowData.cascadeSplits = {
			splitAt(1), splitAt(2), splitAt(3), splitAt(4)
		};
		cascadedShadowData.cascadeSplits2 = {
			splitAt(5), splitAt(6), 1.0e6f,
			ECSHandler::getSystem<SystemsModule::RenderSystem>()->isShadowsDebugData() ? 1.f : 0.f
		};

		rhi->updateBuffer(cascadedShadowsUBO, cascadedShadowData);

		const auto& mats = csm->lightSpaceMatrices;
		if (!mats.empty()) {
			Math::Mat4 packed[6]{};
			const size_t n = (std::min)(mats.size(), size_t{6});
			for (size_t i = 0; i < 6; ++i) {
				packed[i] = mats[i < n ? i : n - 1];
			}
			rhi->updateBuffer(lightMatricesUBO, 0, sizeof(packed), packed);
		}
	}

	drawData.fogEnd = Engine::instance()->getWindow()->getScreenData().far;
	drawData.fogStart = Engine::instance()->getWindow()->getScreenData().far * renderDataHandle.graphics.fogStart;
	drawData.ssaoAmount = renderDataHandle.graphics.ssao ? 1.f : 0.f;
	drawData.viewPosX = renderDataHandle.mCameraPos.x;
	drawData.viewPosY = renderDataHandle.mCameraPos.y;
	drawData.viewPosZ = renderDataHandle.mCameraPos.z;
	drawData.viewDirX = renderDataHandle.mViewDir.x;
	drawData.viewDirY = renderDataHandle.mViewDir.y;
	drawData.viewDirZ = renderDataHandle.mViewDir.z;
	drawData.iblStrength = renderDataHandle.graphics.iblStrength;
	drawData.shadowAmbient = renderDataHandle.graphics.shadowAmbient;
	drawData.padLook0 = renderDataHandle.graphics.sssWrap;
	drawData.padLook1 = renderDataHandle.graphics.sssStrength;
	drawData.debugTerm = static_cast<float>(Engine::instance()->lightingDebugTerm());
	rhi->updateBuffer(pushConstantsUBO, drawData);

	const int write = 1 - mPresentShow;

	RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = mPresentFbo[write];
	beginInfo.renderPass = renderPass;
	beginInfo.width = Engine::instance()->getWindow()->getScreenData().renderW;
	beginInfo.height = Engine::instance()->getWindow()->getScreenData().renderH;

	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, pipeline);
	updateTextures(renderDataHandle, cmd);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	mPresentShow = write;
	renderDataHandle.frame.set(FrameId::Lit, { .texture = mPresentTex[write], .sampler = mPresentSampler });
	renderDataHandle.frame.set(FrameId::Final, { .texture = mPresentTex[write], .sampler = mPresentSampler, .framebuffer = mPresentFbo[write] });
}

void LightingPass::updateTextures(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd)
{
	auto rhi = Engine::instance()->rhi();
	const auto uboSet = mUboSet;

	std::vector<Render::WriteDescriptor> writes;
	auto& frame = renderDataHandle.frame;
	const auto* point = frame.data<PointShadowFrame>(FrameId::PointShadow);
	const auto* csm = frame.data<CsmFrame>(FrameId::Csm);

	auto addImage = [&](uint32_t binding, TextureHandle tex, SamplerHandle samp, BoundImage& prev) {
		if (tex.id == prev.texture.id && samp.id == prev.sampler.id) {
			return;
		}
		prev.texture = tex;
		prev.sampler = samp;
		if (!tex) {
			return;
		}
		writes.push_back({
			.set = mTexSet,
			.binding = binding,
			
			.images{ {tex, samp} }
		});
	};
	auto addBuffer = [&](uint32_t binding, BufferHandle buf, BufferHandle& prev) {
		if (buf.id == prev.id) {
			return;
		}
		prev = buf;
		if (!buf) {
			return;
		}
		writes.push_back({
			.set = uboSet,
			.binding = binding,
			
			.buffers{ {buf} }
		});
	};

	addImage(0, frame.texture(FrameId::GBufferPosition), frame.sampler(FrameId::GBufferPosition), mBoundGPos);
	addBuffer(11, lightMatricesUBO, mBoundLightMat);
	addImage(1, frame.texture(FrameId::GBufferNormal), frame.sampler(FrameId::GBufferNormal), mBoundGNormal);
	addImage(2, frame.texture(FrameId::GBufferAlbedo), frame.sampler(FrameId::GBufferAlbedo), mBoundGAlbedo);

	{
		Render::TextureHandle ssaoTex = frame.texture(FrameId::SsaoBlur);
		Render::SamplerHandle ssaoSampler = frame.sampler(FrameId::SsaoBlur);
		if (!ssaoTex) {
			if (const auto* white = AssetsModule::TextureHandler::instance()->loadTexture("white.png")) {
				ssaoTex = white->texture;
				ssaoSampler = white->sampler;
			}
		}
		addImage(3, ssaoTex, ssaoSampler, mBoundSsao);
	}

	if (point) {
		addBuffer(3, point->matricesUBO, mBoundPointMat);
	}
	addImage(7, frame.texture(FrameId::ShadowPoint), frame.sampler(FrameId::ShadowPoint), mBoundPointShadow);

	{
		Render::TextureHandle outlinesTex = frame.texture(FrameId::GBufferOutlines);
		Render::SamplerHandle outlinesSampler = frame.sampler(FrameId::GBufferOutlines);
		if (!outlinesTex) {
			if (const auto* white = AssetsModule::TextureHandler::instance()->loadTexture("white.png")) {
				outlinesTex = white->texture;
				outlinesSampler = white->sampler;
			}
		}
		addImage(5, outlinesTex, outlinesSampler, mBoundOutlines);
	}

	if (csm) {
		addImage(8, frame.texture(FrameId::ShadowCsm), frame.sampler(FrameId::ShadowCsm), mBoundCsm);
		addImage(6, frame.texture(FrameId::ShadowCsmDepth), frame.sampler(FrameId::ShadowCsmDepth), mBoundCsmDepth);
	}

	addBuffer(15, pushConstantsUBO, mBoundPc);
	addBuffer(5, renderDataHandle.cameraMatricesUBO, mBoundCam);

	if (mIbl.irradiance && mIbl.prefilter && mIbl.brdfLut) {
		addImage(12, mIbl.irradiance, mIbl.irradianceSampler, mBoundIblIrr);
		addImage(13, mIbl.prefilter, mIbl.prefilterSampler, mBoundIblPref);
		addImage(14, mIbl.brdfLut, mIbl.lutSampler, mBoundIblLut);
	}

	if (!writes.empty()) {
		rhi->updateDescriptorSets(writes);
	}
	Render::DescriptorSetHandle sets[2] = { mTexSet, mUboSet };
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, sets, 2);
}
