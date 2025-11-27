#include "LightingPass.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "systemsModule/systems/RenderSystem.h"
#include "imgui.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "renderModule/SceneGridFloor.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;


LightingPass::LightingPass() {

}

void LightingPass::init()
{
	RenderPass::init();
	
	auto* rhi = Engine::instance()->rhi();

	// Описываем набор текстур, которые используются в deferred_shading.fs
	Render::DescriptorSetLayoutDesc texLayoutDesc;
	texLayoutDesc.bindings.push_back({
		.binding = 0,
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 1,
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 2,
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 3,
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 4,
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});

	// Если будешь тащить сюда shadow map’ы:
	texLayoutDesc.bindings.push_back({
		.binding = 30, // PointLightShadowMapArray
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 31, // каскадные тени
		.type = Render::DescriptorType::CombinedImageSampler,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
		});
	texLayoutDesc.bindings.push_back({
		.binding = 10,
		.type = Render::DescriptorType::UniformBuffer,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
	});
	texLayoutDesc.bindings.push_back({
		.binding = 9,
		.type = Render::DescriptorType::UniformBuffer,
		.count = 1,
		.stageMask = Render::ShaderStage::Fragment
	});

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
	pipeline = rhi->createGraphicsPipeline(pipelineDesc);

	RenderPassDesc rpDesc;
	rpDesc.colorAttachmentCount = 1;
	rpDesc.colorAttachments[0] = {
		.loadOp = AttachmentLoadOp::Clear,
		.storeOp = AttachmentStoreOp::Store,
		.clearColor = Math::Vec4{0.f, 0.f, 0.f, 1.f}
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
		drawData.ambientColor = renderDataHandle.mCascadedShadowsPassData->lightColor; //todo 0 for night and 1 for day, some time system
		drawData.cascadeCount = static_cast<int>(renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels.size());
		drawData.shadowIntensity = renderDataHandle.mCascadedShadowsPassData->shadowsIntensity;

		cascadedShadowData.direction_intensity.xyz = renderDataHandle.mCascadedShadowsPassData->lightDirection;
		cascadedShadowData.direction_intensity.w = 0.6f;
		cascadedShadowData.color_and_misc.xyz = renderDataHandle.mCascadedShadowsPassData->lightColor;
	
		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData->shadowCascades.size(); ++i) {
			auto& cascade = renderDataHandle.mCascadedShadowsPassData->shadowCascades[i];
			auto& cascadeRenderData = cascadedShadowData.data[i];

			cascadeRenderData.cascade.xy = cascade.texelSize;
			cascadeRenderData.cascade.z = cascade.bias;
			cascadeRenderData.cascade.w = static_cast<float>(cascade.samples);
		}

		rhi->updateBuffer(cascadedShadowsUBO, cascadedShadowData);
	}

	drawData.fogEnd = Engine::instance()->getWindow()->getScreenData().far;
	drawData.fogStart = Engine::instance()->getWindow()->getScreenData().far * 0.9f;
	drawData.viewPos = renderDataHandle.mCameraPos;

	RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = renderPass;
	beginInfo.width = Engine::instance()->getWindow()->getScreenData().width;
	beginInfo.height = Engine::instance()->getWindow()->getScreenData().height;

	rhi->cmdBeginRenderPass(cmd, beginInfo);
	rhi->cmdBindPipeline(cmd, pipeline);
	updateTextures(renderDataHandle, cmd);

	rhi->cmdPushConstants(cmd, pipeline, ShaderStage::Fragment, &drawData, sizeof(drawData));
	Utils::renderQuad();

	// 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	int w = screen.renderW;
	int h = screen.renderH;

	rhi->blitFramebuffer(
		renderDataHandle.mGeometryPassData->gFramebuffer, // src
		{},                                               // dst = default framebuffer
		0, 0, w, h,
		0, 0, w, h,
		BlitMask::Depth,
		BlitFilter::Nearest
	);

	rhi->cmdEndRenderPass(cmd);

	//drawSky(renderDataHandle);
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

	// Cascaded shadow map (binding 31) - если нужен отдельной текстурой
	if (renderDataHandle.mCascadedShadowsPassData) {
		Render::WriteDescriptor w{};
		w.set = mTexSet;
		w.binding = 31;
		w.type = Render::DescriptorType::CombinedImageSampler;
		w.images.push_back({
			renderDataHandle.mCascadedShadowsPassData->shadowMapTexture,
			renderDataHandle.mCascadedShadowsPassData->shadowMapSampler
			});
		writes.push_back(std::move(w));
	}

	// Обновляем дескрипторы
	if (!writes.empty()) {
		rhi->updateDescriptorSets(writes);
	}
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &mTexSet, 1);
}

void LightingPass::drawSky(SystemsModule::RenderData& renderDataHandle)
{
	static Math::Vec3 sunDir;
	static float time = 0.f;
	static float Br = 0.005f;
	static float Bm = 0.001f;
	static float g = 0.9900f;
	static float cirrus = 0.0f;
	static float cumulus = 0.0f;
	static bool enableSky = 0.0f;
	time += 0.01f;

	{
		/*if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Debug")) {
				ImGui::Checkbox("skyParams", &skyParams);
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();*/
	}
	if (skyParams) {
		if (ImGui::Begin("sky params", &skyParams)) {
			float sunDirCont[3] = { sunDir.x, sunDir.y, sunDir.z };
			if (ImGui::DragFloat3("sun_Dir", sunDirCont, 0.01f, 0.f, 1.f)) {
				sunDir.x = sunDirCont[0];
				sunDir.y = sunDirCont[1];
				sunDir.z = sunDirCont[2];
			}
			ImGui::Checkbox("skyEnabled", &enableSky);
			ImGui::DragFloat("time", &time, 0.01f);
			ImGui::DragFloat("Br", &Br, 0.0001f);
			ImGui::DragFloat("Bm", &Bm, 0.0001f);
			ImGui::DragFloat("g", &g, 0.01f);
			ImGui::DragFloat("cirrus", &cirrus, 0.01f);
			ImGui::DragFloat("cumulus", &cumulus, 0.01f);

		}
		ImGui::End();
	}


	if (enableSky) {
		auto sky = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/sky.vs", "shaders/sky.fs");
		sky->use();
		sky->setUniform("view", renderDataHandle.current.view);
		sky->setUniform("projection", renderDataHandle.current.projection);
		sky->setUniform("sun_direction", -renderDataHandle.mCascadedShadowsPassData->lightDirection);
		sky->setUniform("time", time);

		sky->setUniform("Br", Br);
		sky->setUniform("Bm", Bm);
		sky->setUniform("g", g);

		sky->setUniform("cirrus", cirrus);
		sky->setUniform("cumulus", cumulus);

		Utils::renderQuad();
		//Utils::renderQuad2();
	}
}
