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

void LightingPass::render(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK;
	GLW::clear(GLW::ColorBit::DEPTH_COLOR);

	auto shaderLightingPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");

	shaderLightingPass->use();
	shaderLightingPass->setUniform("gPosition", 0);
	shaderLightingPass->setUniform("gNormal", 1);
	shaderLightingPass->setUniform("gAlbedoSpec", 2);
	shaderLightingPass->setUniform("ssao", 3);
	shaderLightingPass->setUniform("shadows", 4);
	shaderLightingPass->setUniform("gOutlines", 5);

	shaderLightingPass->setUniform("pointLightsSize", static_cast<int>(renderDataHandle.mPointPassData->shadowEntities.size()));
	shaderLightingPass->setUniform("PointLightShadowMapArray", 30);

	shaderLightingPass->setUniform("fogStart", Engine::instance()->getWindow()->getScreenData().far * 0.9f);
	shaderLightingPass->setUniform("screenDrawData.far", Engine::instance()->getWindow()->getScreenData().far);

	int offsetSum = 0;
	for (size_t i = 0; i < renderDataHandle.mPointPassData->shadowEntities.size(); i++) {
		auto tc = ECSHandler::registry().getComponent<TransformComponent>(renderDataHandle.mPointPassData->shadowEntities[i]);
		auto lightComp = ECSHandler::registry().getComponent<LightSourceComponent>(renderDataHandle.mPointPassData->shadowEntities[i]);

		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Position").c_str(), tc->getPos(true));
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].texelSize").c_str(), Math::Vec2{lightComp->getTexelSize().x, lightComp->getTexelSize().y});
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].bias").c_str(), lightComp->getBias());
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].samples").c_str(), lightComp->getSamples());
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].radius").c_str(), lightComp->mRadius);
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].offset").c_str(), offsetSum);

		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Color").c_str(), lightComp->getLightColor());

		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Linear").c_str(), lightComp->mLinear);
		shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Quadratic").c_str(), lightComp->mQuadratic);

		if (lightComp->getType() == ComponentsModule::eLightType::POINT) {
			shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Type").c_str(), 0);
			shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Layers").c_str(), 6);
		}
		else {
			shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Type").c_str(), 1);
			shaderLightingPass->setUniform(("pointLight[" + std::to_string(i) + "].Layers").c_str(), 1);
		}


		offsetSum += lightComp->getTypeOffset(lightComp->getType());
	}

	if (!renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels.empty()) {

		shaderLightingPass->setUniform("ambientColor", renderDataHandle.mCascadedShadowsPassData->lightColor);//todo 0 for night and 1 for day, some time system
		GLW::bindTextureToSlot(31, GLW::TEXTURE_2D_ARRAY, renderDataHandle.mCascadedShadowsPassData->shadowMapTexture);
		shaderLightingPass->setUniform("cascadedShadow.shadowMap", 31);
		shaderLightingPass->setUniform("cascadedShadow.direction", renderDataHandle.mCascadedShadowsPassData->lightDirection);
		shaderLightingPass->setUniform("cascadedShadow.color", renderDataHandle.mCascadedShadowsPassData->lightColor);
		shaderLightingPass->setUniform("cascadeCount", static_cast<int>(renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels.size()));
		shaderLightingPass->setUniform("shadowIntensity", renderDataHandle.mCascadedShadowsPassData->shadowsIntensity);

		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels.size(); ++i) {
			shaderLightingPass->setUniform(("cascadePlaneDistances[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData->shadowCascadeLevels[i]);
		}

		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData->shadowCascades.size(); ++i) {
			shaderLightingPass->setUniform(("cascadedShadow.texelSize[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData->shadowCascades[i].texelSize);
			shaderLightingPass->setUniform(("cascadedShadow.bias[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData->shadowCascades[i].bias);
			shaderLightingPass->setUniform(("cascadedShadow.samples[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData->shadowCascades[i].samples);
		}
	}

	// set light uniforms
	shaderLightingPass->setUniform("viewPos", renderDataHandle.mCameraPos);

	GLW::bindTextureToSlot(0, &renderDataHandle.mGeometryPassData->positionBuffer);
	GLW::bindTextureToSlot(1, &renderDataHandle.mGeometryPassData->normalBuffer);
	GLW::bindTextureToSlot(2, &renderDataHandle.mGeometryPassData->albedoBuffer);
	GLW::bindTextureToSlot(3, &renderDataHandle.mSSAOPassData->mSsaoColorBufferBlur);
	GLW::bindTextureToSlot(5, &renderDataHandle.mGeometryPassData->outlinesBuffer);

	Utils::renderQuad();

	// 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
	GLW::bindReadFramebuffer(renderDataHandle.mGeometryPassData->gFramebuffer.id);
	if (auto curFBO = GLW::FramebufferStack::top()) {
		GLW::bindDrawFramebuffer(*curFBO);
		const auto w = Engine::instance()->getWindow()->getScreenData().renderW;
		const auto h = Engine::instance()->getWindow()->getScreenData().renderH;
		GLW::blitFramebuffer(0, 0, w, h, 0, 0, w, h, GLW::ColorBit::DEPTH, GLW::BlitFilter::NEAREST);
		GLW::bindReadFramebuffer(*curFBO);
	}
	else {
		GLW::bindDrawFramebuffer(); // write to default framebuffer
		const auto w = Engine::instance()->getWindow()->getScreenData().renderW;
		const auto h = Engine::instance()->getWindow()->getScreenData().renderH;
		GLW::blitFramebuffer(0, 0, w, h, 0, 0, w, h, GLW::ColorBit::DEPTH, GLW::BlitFilter::NEAREST);
		GLW::bindReadFramebuffer();
	}

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
}
