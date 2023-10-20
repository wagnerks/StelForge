#include "LightingPass.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "systemsModule/RenderSystem.h"
#include "imgui.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "renderModule/SceneGridFloor.h"

using namespace Engine::RenderModule::RenderPasses;

void LightingPass::init() {}

void LightingPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	if (!renderer) {
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	auto shaderLightingPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_shading.vs", "shaders/deferred_shading.fs");

	shaderLightingPass->use();
	shaderLightingPass->setInt("gPosition", 0);
	shaderLightingPass->setInt("gNormal", 1);
	shaderLightingPass->setInt("gAlbedoSpec", 2);
	shaderLightingPass->setInt("ssao", 3);
	shaderLightingPass->setInt("shadows", 4);
	shaderLightingPass->setInt("gOutlines", 5);

	shaderLightingPass->setInt("pointLightsSize", static_cast<int>(renderDataHandle.mPointPassData.shadowEntities.size()));
	shaderLightingPass->setInt("PointLightShadowMapArray", 30);

	int offsetSum = 0;
	for (size_t i = 0; i < renderDataHandle.mPointPassData.shadowEntities.size(); i++) {
		auto tc = ECSHandler::registry()->getComponent<TransformComponent>(renderDataHandle.mPointPassData.shadowEntities[i]);
		auto lightComp = ECSHandler::registry()->getComponent<LightSourceComponent>(renderDataHandle.mPointPassData.shadowEntities[i]);

		shaderLightingPass->setVec3(("pointLight[" + std::to_string(i) + "].Position").c_str(), tc->getPos(true));
		shaderLightingPass->setVec2(("pointLight[" + std::to_string(i) + "].texelSize").c_str(), lightComp->getTexelSize());
		shaderLightingPass->setFloat(("pointLight[" + std::to_string(i) + "].bias").c_str(), lightComp->getBias());
		shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].samples").c_str(), lightComp->getSamples());
		shaderLightingPass->setFloat(("pointLight[" + std::to_string(i) + "].radius").c_str(), lightComp->mRadius);
		shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].offset").c_str(), offsetSum);

		shaderLightingPass->setVec3(("pointLight[" + std::to_string(i) + "].Color").c_str(), lightComp->getLightColor());

		shaderLightingPass->setFloat(("pointLight[" + std::to_string(i) + "].Linear").c_str(), lightComp->mLinear);
		shaderLightingPass->setFloat(("pointLight[" + std::to_string(i) + "].Quadratic").c_str(), lightComp->mQuadratic);

		if (lightComp->getType() == ComponentsModule::eLightType::POINT) {
			shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].Type").c_str(), 0);
			shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].Layers").c_str(), 6);
		}
		else {
			shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].Type").c_str(), 1);
			shaderLightingPass->setInt(("pointLight[" + std::to_string(i) + "].Layers").c_str(), 1);
		}


		offsetSum += lightComp->getTypeOffset(lightComp->getType());
	}

	if (!renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.empty()) {
		shaderLightingPass->setFloat("ambientColor", 1.f);//todo 0 for night and 1 for day, some time system
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE31, GL_TEXTURE_2D_ARRAY, renderDataHandle.mCascadedShadowsPassData.shadowMapTexture);
		shaderLightingPass->setInt("cascadedShadow.shadowMap", 31);
		shaderLightingPass->setVec3("cascadedShadow.direction", renderDataHandle.mCascadedShadowsPassData.lightDirection);
		shaderLightingPass->setVec3("cascadedShadow.color", renderDataHandle.mCascadedShadowsPassData.lightColor);
		shaderLightingPass->setInt("cascadeCount", static_cast<int>(renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.size()));
		shaderLightingPass->setFloat("shadowIntensity", renderDataHandle.mCascadedShadowsPassData.shadowsIntensity);

		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.size(); ++i) {
			shaderLightingPass->setFloat(("cascadePlaneDistances[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels[i]);
		}

		for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData.shadowCascades.size(); ++i) {
			shaderLightingPass->setVec2(("cascadedShadow.texelSize[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData.shadowCascades[i].texelSize);
			shaderLightingPass->setFloat(("cascadedShadow.bias[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData.shadowCascades[i].bias);
			shaderLightingPass->setInt(("cascadedShadow.samples[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData.shadowCascades[i].samples);
		}
	}

	// set light uniforms
	shaderLightingPass->setVec3("viewPos", renderDataHandle.mCameraPos);


	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gAlbedoSpec);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE3, GL_TEXTURE_2D, renderDataHandle.mSSAOPassData.mSsaoColorBufferBlur);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE5, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gOutlines);

	// finally render quad
	Utils::renderQuad();

	// 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
	// ----------------------------------------------------------------------------------
	glBindFramebuffer(GL_READ_FRAMEBUFFER, renderDataHandle.mGeometryPassData.mGBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
	glBlitFramebuffer(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);



	static glm::vec3 sunDir;
	static float time = 0.f;
	static float Br = 0.005f;
	static float Bm = 0.001f;
	static float g = 0.9900f;
	static float cirrus = 0.0f;
	static float cumulus = 0.0f;
	static bool enableSky = 0.0f;
	time += 0.01f;

	if (ImGui::Begin("sky params")) {
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

	if (enableSky) {
		auto sky = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/sky.vs", "shaders/sky.fs");
		sky->use();
		sky->setMat4("view", renderDataHandle.mView);
		sky->setMat4("projection", renderDataHandle.mProjection);
		sky->setVec3("sun_direction", -renderDataHandle.mCascadedShadowsPassData.lightDirection);
		sky->setFloat("time", time);

		sky->setFloat("Br", Br);
		sky->setFloat("Bm", Bm);
		sky->setFloat("g", g);

		sky->setFloat("cirrus", cirrus);
		sky->setFloat("cumulus", cumulus);

		Utils::renderQuad();
	}

}
