#include "LightingPass.h"
#include "assetsModule/TextureHandler.h"
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

	texLayoutDesc.bindings.emplace_back(7, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);
	texLayoutDesc.bindings.emplace_back(8, Render::DescriptorType::CombinedImageSampler, Render::ShaderStage::Fragment);


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
	mLitTexture = rhi->createTexture(litDesc);
	mPresentTex[0] = rhi->createTexture(litDesc);
	mPresentTex[1] = rhi->createTexture(litDesc);

	fbDesc.colorAttachments.push_back({ mLitTexture });
	fbDesc.depthStencilAttachment = {.texture = renderDataHandle.mGeometryPassData->depthsBuffer };
	lightingFBO = rhi->createFramebuffer(fbDesc);

	for (int i = 0; i < 2; ++i) {
		FramebufferDesc presentFb = fbDesc;
		presentFb.depthStencilAttachment.reset();
		presentFb.colorAttachments = { { mPresentTex[i] } };
		mPresentFbo[i] = rhi->createFramebuffer(presentFb);
	}
	renderDataHandle.finalTexture = mPresentTex[0];

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
	plubodesc.size = sizeof(pointLight); //max 6 lights

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
}

void LightingPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	assert(mTexSetInitialized);
	FUNCTION_BENCHMARK;

	auto* rhi = Engine::instance()->rhi();
	int offsetSum = 0;
	if (renderDataHandle.mPointPassData) {
		for (size_t i = 0; i < renderDataHandle.mPointPassData->shadowEntities.size(); i++) {
			auto tc = ECSHandler::registry().pinComponent<const TransformComponent>(renderDataHandle.mPointPassData->shadowEntities[i]);
			auto lightComp = ECSHandler::registry().pinComponent<const LightSourceComponent>(renderDataHandle.mPointPassData->shadowEntities[i]);

			auto& light = pointLight[i];

			light.Position = tc->getPos(true);
			light.texelSize = Math::Vec2{ lightComp->getTexelSize().x, lightComp->getTexelSize().y };
			light.bias = lightComp->getBias();
			light.samples = lightComp->getSamples();
			light.radius = lightComp->mRadius;
			light.offset = offsetSum;

			light.Color = lightComp->getLightColor();
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
		drawData.pointLightsSize = renderDataHandle.mPointPassData ? static_cast<int>(renderDataHandle.mPointPassData->shadowEntities.size()) : 0;
	}

	if (renderDataHandle.mCascadedShadowsPassData && !renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels.empty()) {
		const auto& lightColor = renderDataHandle.mCascadedShadowsPassData->lightColor;
		drawData.ambientX = lightColor.x;
		drawData.ambientY = lightColor.y;
		drawData.ambientZ = lightColor.z;
		drawData.cascadeCount = static_cast<int>(renderDataHandle.mCascadedShadowsPassData->shadowCascades.size());
		drawData.shadowIntensity = renderDataHandle.mCascadedShadowsPassData->shadowsIntensity;

		cascadedShadowData.direction_intensity.xyz = renderDataHandle.mCascadedShadowsPassData->lightDirection;
		cascadedShadowData.direction_intensity.w = renderDataHandle.mCascadedShadowsPassData->shadowsIntensity;
		cascadedShadowData.color_and_misc.xyz = renderDataHandle.mCascadedShadowsPassData->lightColor;
		cascadedShadowData.color_and_misc.w = static_cast<float>(drawData.cascadeCount);
	
		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData->shadowCascades.size(); ++i) {
			auto& cascade = renderDataHandle.mCascadedShadowsPassData->shadowCascades[i];
			auto& cascadeRenderData = cascadedShadowData.data[i];

			cascadeRenderData.cascade.xy = cascade.texelSize;
			cascadeRenderData.cascade.z = cascade.bias;
			cascadeRenderData.cascade.w = static_cast<float>(cascade.samples);
		}

		const auto& levels = renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels;
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

		const auto& mats = renderDataHandle.mCascadedShadowsPassData->lightSpaceMatrices;
		if (!mats.empty()) {
			rhi->updateBuffer(lightMatricesUBO, 0, mats.size() * sizeof(Math::Mat4), mats.data());
		}
	}

	drawData.fogEnd = Engine::instance()->getWindow()->getScreenData().far;
	drawData.fogStart = Engine::instance()->getWindow()->getScreenData().far * 0.9f;
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
	renderDataHandle.finalTexture = mPresentTex[1 - write];
}

void LightingPass::updateTextures(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd)
{
	auto rhi = Engine::instance()->rhi();

	std::vector<Render::WriteDescriptor> writes;

	// gPosition (binding 0)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 0;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			renderDataHandle.mGeometryPassData->positionBuffer,
			renderDataHandle.mGeometryPassData->sampler
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
			renderDataHandle.mGeometryPassData->normalBuffer,
			renderDataHandle.mGeometryPassData->sampler
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
			renderDataHandle.mGeometryPassData->albedoBuffer,
			renderDataHandle.mGeometryPassData->sampler
			});
		writes.push_back(std::move(w));
	}

	// ssao (binding 3)
	{
		Render::TextureHandle ssaoTex{};
		Render::SamplerHandle ssaoSampler{};

		if (renderDataHandle.mSSAOPassData) {
			ssaoTex = renderDataHandle.mSSAOPassData->mSsaoColorBufferBlur;
			ssaoSampler = renderDataHandle.mSSAOPassData->sampler;
		}

		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 3;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({ ssaoTex, ssaoSampler });
		writes.push_back(std::move(w));
	}

	if (renderDataHandle.mPointPassData && renderDataHandle.mPointPassData->matricesUBO) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 3;
		w.type = Render::DescriptorType::UniformBuffer;
		w.buffers.push_back({ renderDataHandle.mPointPassData->matricesUBO });
		writes.push_back(std::move(w));
	}

	// PointLightShadowMapArray (binding 4) - если нужен
	if (renderDataHandle.mPointPassData) {
		/*Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 4;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			renderDataHandle.mPointPassData->shadowMapArrayTexture,
			renderDataHandle.mPointPassData->shadowMapArraySampler
			});
		writes.push_back(std::move(w));*/
	}

	// gOutlines (binding 5)
	{
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 5;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			renderDataHandle.mGeometryPassData->outlinesBuffer,
			renderDataHandle.mGeometryPassData->sampler
			});
		writes.push_back(std::move(w));
	}

	// Cascaded shadow map
	if (renderDataHandle.mCascadedShadowsPassData) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 8;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			renderDataHandle.mCascadedShadowsPassData->shadowMapTexture,
			renderDataHandle.mCascadedShadowsPassData->shadowMapSampler
			});
		writes.push_back(std::move(w));
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

	// Обновляем дескрипторы
	if (!writes.empty()) {
		rhi->updateDescriptorSets(writes);
	}
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);
}
