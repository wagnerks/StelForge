#include "LightingPass.h"
#include <algorithm>
#include "assetsModule/TextureHandler.h"
#include "renderModule/IblBaker.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"
#include "imgui.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
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

	// Описываем набор текстур, которые используются в deferred_shading.fs
	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(1, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(2, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(4, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);

	texLayoutDesc.bindings.emplace_back(0, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(3, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(9, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(10, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(5, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(11, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(15, Render::DescriptorType::UniformBuffer, Render::ShaderStage::Fragment);

	texLayoutDesc.bindings.emplace_back(6, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(7, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(8, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(12, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(13, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(14, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);


	mTexSetLayout = rhi->createDescriptorSetLayout(texLayoutDesc);
	mTexSet = rhi->allocateDescriptorSet(mTexSetLayout);

	mTexSetInitialized = true;

	GraphicsPipelineDesc pipelineDesc{};
	Render::VertexLayoutDesc& layout = pipelineDesc.layout;
	layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), Render::VertexInputRate::PerVertex);
	layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, position)), false);
	layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<std::uint32_t>(offsetof(Vertex2D, texCoords)), false);

	pipelineDesc.shaders.vertex = rhi->createShaderModule({ ShaderStage::Vertex, "deferred shading vs", ShaderModule::ShaderBase::loadShaderCode("shaders/deferred_shading.vs") });
	pipelineDesc.shaders.fragment = rhi->createShaderModule({ShaderStage::Fragment, "deferred shading fs", ShaderModule::ShaderBase::loadShaderCode("shaders/deferred_shading.fs")});
	pipelineDesc.topology = PrimitiveTopology::Triangles;
	pipelineDesc.cullMode = CullMode::None;
	pipelineDesc.depthStencilState.depthTest = false;
	pipelineDesc.depthStencilState.depthWrite = false;
	pipeline = rhi->createGraphicsPipeline(pipelineDesc);

	FramebufferDesc fbDesc;
	fbDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	fbDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;

	TextureDesc litDesc;
	litDesc.width = Engine::instance()->getWindow()->getScreenData().renderW;
	litDesc.height = Engine::instance()->getWindow()->getScreenData().renderH;
	litDesc.format = TextureFormat::RGBA16F;
	litDesc.usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	mLitTexture = rhi->createTexture(litDesc);
	mPresentTex[0] = rhi->createTexture(litDesc);
	mPresentTex[1] = rhi->createTexture(litDesc);

	fbDesc.colorAttachments.push_back({ mLitTexture });
	fbDesc.depthStencilAttachment = {.texture = renderDataHandle.frame.texture(FrameId::GBufferDepth) };
	lightingFBO = rhi->createFramebuffer(fbDesc);

	for (int i = 0; i < 2; ++i) {
		FramebufferDesc presentFb = fbDesc;
		presentFb.depthStencilAttachment.reset();
		presentFb.colorAttachments = { { mPresentTex[i] } };
		mPresentFbo[i] = rhi->createFramebuffer(presentFb);
	}
	renderDataHandle.frame.set(FrameId::Lit, { .texture = mLitTexture });
	renderDataHandle.frame.set(FrameId::Final, { .texture = mPresentTex[0] });

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0] = {
		.loadOp = AttachmentLoadOp::Clear,
		.storeOp = AttachmentStoreOp::Store,
		.clearColor = Math::Vec4{0.f, 0.f, 0.f, 1.f}
	};
	rpDesc.depthAttachment =  {
		.loadOp = AttachmentLoadOp::Load,
		.storeOp = AttachmentStoreOp::Store,
		.clearDepth = 1.f,
		.clearStencil = 0,
	};

	renderPass = rhi->createRenderPass(rpDesc);

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

	std::vector<Render::WriteDescriptor> writes{ {
		.set = mTexSet,
		.binding = 10,
		.type = Render::DescriptorType::UniformBuffer,
		.buffers{ {pointLightsUBO} }
	},
	{
		.set = mTexSet,
		.binding = 9,
		.type = Render::DescriptorType::UniformBuffer,
		.buffers{ {cascadedShadowsUBO} }
	},
	{
		.set = mTexSet,
		.binding = 15,
		.type = Render::DescriptorType::UniformBuffer,
		.buffers{ {pushConstantsUBO} }
	},
	{
		.set = mTexSet,
		.binding = 11,
		.type = Render::DescriptorType::UniformBuffer,
		.buffers{ {lightMatricesUBO} }
	} };

	rhi->updateDescriptorSets(writes);

	mIbl = SFE::Render::bakeIbl(rhi);
	if (mIbl.irradiance && mIbl.prefilter && mIbl.brdfLut) {
		std::vector<Render::WriteDescriptor> iblWrites{ {
			.set = mTexSet,
			.binding = 12,
			.type = Render::DescriptorType::CombinedImageSampler,
			.images{ { mIbl.irradiance, mIbl.irradianceSampler } }
		},
		{
			.set = mTexSet,
			.binding = 13,
			.type = Render::DescriptorType::CombinedImageSampler,
			.images{ { mIbl.prefilter, mIbl.prefilterSampler } }
		},
		{
			.set = mTexSet,
			.binding = 14,
			.type = Render::DescriptorType::CombinedImageSampler,
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
			cascadeRenderData.cascade.y = (std::max)(cascade.texelSize.x, invRes * 10.f);
			cascadeRenderData.cascade.z = cascade.bias;
			cascadeRenderData.cascade.w = static_cast<float>(cascade.samples);
		}

		const auto& levels = csm->shadowCascadeLevels;
		auto splitAt = [&levels](size_t i) {
			return (i < levels.size()) ? levels[i] : 1.0e6f;
		};
		cascadedShadowData.cascadeSplits = {
			splitAt(1), splitAt(2), splitAt(3), splitAt(4)
		};
		cascadedShadowData.cascadeSplits2 = {
			splitAt(5), splitAt(6), 1.0e6f, 1.0e6f
		};

		rhi->updateBuffer(cascadedShadowsUBO, cascadedShadowData);

		const auto& mats = csm->lightSpaceMatrices;
		if (!mats.empty()) {
			rhi->updateBuffer(lightMatricesUBO, 0, mats.size() * sizeof(Math::Mat4), mats.data());
		}
	}

	drawData.fogEnd = Engine::instance()->getWindow()->getScreenData().far;
	drawData.fogStart = Engine::instance()->getWindow()->getScreenData().far * renderDataHandle.graphics.fogStart;
	drawData.ssaoAmount = renderDataHandle.graphics.ssao ? 1.f : 0.f;
	drawData.viewPosX = renderDataHandle.mCameraPos.x;
	drawData.viewPosY = renderDataHandle.mCameraPos.y;
	drawData.viewPosZ = renderDataHandle.mCameraPos.z;
	rhi->updateBuffer(pushConstantsUBO, drawData);

	RenderPassBeginInfo beginInfo;
	beginInfo.framebuffer = lightingFBO;
	beginInfo.renderPass = renderPass;
	beginInfo.width = Engine::instance()->getWindow()->getScreenData().renderW;
	beginInfo.height = Engine::instance()->getWindow()->getScreenData().renderH;

	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, pipeline);
	updateTextures(renderDataHandle, cmd);
	rhi->cmdPushConstants(cmd, pipeline, ShaderStage::Fragment, &drawData, sizeof(drawData));
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);

	const int write = 1 - mPresentShow;
	rhi->cmdBlit(cmd, lightingFBO, mPresentFbo[write], BlitMask::Color, BlitFilter::Nearest);
	mPresentShow = write;
	renderDataHandle.frame.set(FrameId::Lit, { .texture = mLitTexture });
	renderDataHandle.frame.set(FrameId::Final, { .texture = mPresentTex[1 - write] });
}

void LightingPass::updateTextures(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd)
{
	auto rhi = Engine::instance()->rhi();

	std::vector<Render::WriteDescriptor> writes;
	auto& frame = renderDataHandle.frame;
	const auto* point = frame.data<PointShadowFrame>(FrameId::PointShadow);
	const auto* csm = frame.data<CsmFrame>(FrameId::Csm);

	// gPosition (binding 0)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 0;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			frame.texture(FrameId::GBufferPosition),
			frame.sampler(FrameId::GBufferPosition)
			});
		writes.push_back(std::move(w));
	}

	if (lightMatricesUBO) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 11;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ lightMatricesUBO });
		writes.push_back(std::move(w));
	}

	// gNormal (binding 1)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 1;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			frame.texture(FrameId::GBufferNormal),
			frame.sampler(FrameId::GBufferNormal)
			});
		writes.push_back(std::move(w));
	}

	// gAlbedoSpec (binding 2)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 2;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			frame.texture(FrameId::GBufferAlbedo),
			frame.sampler(FrameId::GBufferAlbedo)
			});
		writes.push_back(std::move(w));
	}

	// ssao (binding 3)
	{
		Render::TextureHandle ssaoTex{};
		Render::SamplerHandle ssaoSampler{};

		ssaoTex = frame.texture(FrameId::SsaoBlur);
		ssaoSampler = frame.sampler(FrameId::SsaoBlur);

		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 3;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ ssaoTex, ssaoSampler });
		writes.push_back(std::move(w));
	}

	if (point && point->matricesUBO) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 3;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ point->matricesUBO });
		writes.push_back(std::move(w));
	}

	{
		const auto shadowTex = frame.texture(FrameId::ShadowPoint);
		const auto shadowSampler = frame.sampler(FrameId::ShadowPoint);
		if (shadowTex && shadowSampler) {
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 7;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ shadowTex, shadowSampler });
			writes.push_back(std::move(w));
		}
	}

	// gOutlines (binding 5)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 5;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			frame.texture(FrameId::GBufferOutlines),
			frame.sampler(FrameId::GBufferOutlines)
			});
		writes.push_back(std::move(w));
	}

	// Cascaded shadow map: comparison (PCF) + raw depth (PCSS blockers)
	if (csm) {
		{
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 8;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({
				frame.texture(FrameId::ShadowCsm),
				frame.sampler(FrameId::ShadowCsm)
				});
			writes.push_back(std::move(w));
		}
		{
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 6;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({
				frame.texture(FrameId::ShadowCsmDepth),
				frame.sampler(FrameId::ShadowCsmDepth)
				});
			writes.push_back(std::move(w));
		}
	}

	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 15;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ pushConstantsUBO });
		writes.push_back(std::move(w));
	}

	if (renderDataHandle.cameraMatricesUBO) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 5;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.cameraMatricesUBO });
		writes.push_back(std::move(w));
	}

	if (mIbl.irradiance && mIbl.prefilter && mIbl.brdfLut) {
		{
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 12;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mIbl.irradiance, mIbl.irradianceSampler });
			writes.push_back(std::move(w));
		}
		{
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 13;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mIbl.prefilter, mIbl.prefilterSampler });
			writes.push_back(std::move(w));
		}
		{
			Render::WriteDescriptor w{};
			w.set = mTexSet;
			w.binding = 14;
			w.type = Render::DescriptorType::CombinedImageSampler;
			w.images.push_back({ mIbl.brdfLut, mIbl.lutSampler });
			writes.push_back(std::move(w));
		}
	}

	// Обновляем дескрипторы
	if (!writes.empty()) {
		rhi->updateDescriptorSets(writes);
	}
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);
}
