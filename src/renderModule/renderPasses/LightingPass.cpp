#include "LightingPass.h"
#include "renderModule/Renderer.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/RenderSystem.h"

using namespace GameEngine::RenderModule::RenderPasses;

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
	
	if (!renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.empty()) {
		shaderLightingPass->setFloat("bias", renderDataHandle.mCascadedShadowsPassData.bias);
		TextureHandler::getInstance()->bindTexture(GL_TEXTURE31, GL_TEXTURE_2D_ARRAY, renderDataHandle.mCascadedShadowsPassData.shadowMapTexture);
		shaderLightingPass->setInt("cascadedShadow.shadowMap", 31);
		shaderLightingPass->setVec3("cascadedShadow.direction", renderDataHandle.mCascadedShadowsPassData.lightDirection);
		shaderLightingPass->setVec3("cascadedShadow.color", renderDataHandle.mCascadedShadowsPassData.lightColor);
		shaderLightingPass->setVec2("cascadedShadow.texelSize", 1.f / renderDataHandle.mCascadedShadowsPassData.resolution);
		shaderLightingPass->setInt("cascadeCount", static_cast<int>(renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.size()));
	    for (size_t i = 0; i < renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels.size(); ++i) {
	        shaderLightingPass->setFloat(("cascadePlaneDistances[" + std::to_string(i) + "]").c_str(), renderDataHandle.mCascadedShadowsPassData.shadowCascadeLevels[i]);
	    }
		shaderLightingPass->setFloat("farPlane", renderDataHandle.mCascadedShadowsPassData.cameraFarPlane);
	}


    shaderLightingPass->setMat4("view", renderDataHandle.view);
    // set light uniforms
	shaderLightingPass->setVec3("viewPos", renderDataHandle.cameraPos);


	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gPosition);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gAlbedoSpec);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE3, GL_TEXTURE_2D, renderDataHandle.mSSAOPassData.mSsaoColorBufferBlur);

	//shaderLightingPass->setInt("shadowsCount", static_cast<int>(lightsObj.size()));
	//for (auto i = 0u; i < lightsObj.size(); i++) {
	//	auto lightSource = lightsObj[i];
	//	shaderLightingPass->setMat4(("DirLights[" + std::to_string(i) + "].PV").c_str(), lightSource->getProjectionViewMatrix());
	//	shaderLightingPass->setVec3(("DirLights[" + std::to_string(i) + "].Position").c_str(), lightSource->getComponent<TransformComponent>()->getPos());
	//	shaderLightingPass->setInt(("DirLights[" + std::to_string(i) + "].shadowsMap").c_str(), i + 3);
	//	TextureHandler::getInstance()->bindTexture(GL_TEXTURE3 + i, GL_TEXTURE_2D, lightSource->getDepthMapTexture());
	//}

	//
	//shaderLightingPass->setInt("lightsCount", static_cast<int>(lightPositions.size()));

	//for (unsigned int i = 0; i < lightPositions.size(); i++) {
	//	shaderLightingPass->setVec3(("lights[" + std::to_string(i) + "].Position").c_str(), lightPositions[i]);
	//	shaderLightingPass->setVec3(("lights[" + std::to_string(i) + "].Color").c_str(), lightColors[i]);
	//	// update attenuation parameters and calculate radius
	//	const float constant = 1.0f;
	//	// note that we don't send this to the shader, we assume it is always 1.0 (in our case)
	//	const float linear = 0.7f;
	//	const float quadratic = 1.8f;
	//	shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Linear").c_str(), linear);
	//	shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Quadratic").c_str(), quadratic);
	//	// then calculate radius of light volume/sphere
	//	const float maxBrightness = std::fmaxf(std::fmaxf(lightColors[i].r, lightColors[i].g), lightColors[i].b);
	//	float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
	//	shaderLightingPass->setFloat(("lights[" + std::to_string(i) + "].Radius").c_str(), radius);
	//}
    
    // finally render quad
    Utils::renderQuad();
	
    // 2.5. copy content of geometry's depth buffer to default framebuffer's depth buffer
    // ----------------------------------------------------------------------------------
    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderDataHandle.mGeometryPassData.mGBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
    glBlitFramebuffer(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/*auto sh = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugQuadDepth.vs", "shaders/debugQuadDepth.fs");
	sh->use();
	sh->setInt("depthMap", 31);
	sh->setFloat("near_plane",0.1f);
	sh->setFloat("far_plane",500.f/25.f);
	sh->setInt("layer",0);

	Utils::renderQuad(0.0f, 0.0f, 1.f, 1.f);*/

}
