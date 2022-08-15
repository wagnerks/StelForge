#include "SSAOPass.h"

#include <gtc/random.hpp>

#include "imgui.h"
#include "renderModule/Renderer.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/RenderSystem.h"

using namespace GameEngine::RenderModule::RenderPasses;

float lerp(float a, float b, float f) {
	return a + f * (b - a);
}

void SSAOPass::init() {

	glGenFramebuffers(1, &ssaoFBO);
	glGenFramebuffers(1, &ssaoBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

	// SSAO color buffer
	glGenTextures(1, &ssaoColorBuffer);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_FATAL(false, "SSAO Framebuffer not complete!");
	}
	// and blur stage
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
	glGenTextures(1, &ssaoColorBufferBlur);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_FATAL(false, "SSAO Blur Framebuffer not complete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// generate sample kernel
	// ----------------------

	for (unsigned int i = 0; i < 64; ++i) {
		glm::vec3 sample(glm::linearRand(-1.f, 1.f), glm::linearRand(-1.f, 1.f), glm::linearRand(0.f, 1.f));
		sample = glm::normalize(sample);
		sample *= glm::linearRand(0.f, 1.f);
		float scale = float(i) / 64.0f;

		// scale samples s.t. they're more aligned to center of kernel
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		ssaoKernel.push_back(sample);
	}

	// generate noise texture
	// ----------------------
	std::vector<glm::vec3> ssaoNoise;
	for (unsigned int i = 0; i < 16; i++) {
		glm::vec3 noise(glm::linearRand(-1.f, 1.f), glm::linearRand(-1.f, 1.f), 0.0f);
		// rotate around z-axis (in tangent space)
		ssaoNoise.push_back(noise);
	}

	glGenTextures(1, &noiseTexture);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	auto shaderSSAO = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao.fs");
	shaderSSAO->use();
	shaderSSAO->setInt("gPosition", 0);
	shaderSSAO->setInt("gNormal", 1);
	shaderSSAO->setInt("texNoise", 2);
	shaderSSAO->setInt("kernelSize", kernelSize);
	shaderSSAO->setFloat("radius", radius);
	shaderSSAO->setFloat("bias", bias);
	shaderSSAO->setVec2("noiseScale", glm::vec2(Renderer::SCR_WIDTH / 4.f, Renderer::SCR_HEIGHT / 4.f));
	for (unsigned int i = 0; i < 64; ++i) {
		shaderSSAO->setVec3(("samples[" + std::to_string(i) + "]").c_str(), ssaoKernel[i]);
	}

	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");
	shaderSSAOBlur->use();
	shaderSSAOBlur->setInt("ssaoInput", 0);
}

void SSAOPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	auto shaderSSAO = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao.fs");
	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");

	if (ImGui::Begin("SSAO")) {
		shaderSSAO->use();
		if (ImGui::DragInt("kernelSize", &kernelSize)) {
			shaderSSAO->setInt("kernelSize", kernelSize);
		}
		if (ImGui::DragFloat("radius", &radius, 0.01f)) {
			shaderSSAO->setFloat("radius", radius);
		}
		if (ImGui::DragFloat("bias", &bias, 0.001f)) {
			shaderSSAO->setFloat("bias", bias);
		}
	}
	ImGui::End();

	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
	glClear(GL_COLOR_BUFFER_BIT);
	shaderSSAO->use();
	shaderSSAO->setMat4("projection", renderDataHandle.projection);

	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gViewPosition);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, noiseTexture);
	Utils::renderQuad();
	

	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
	glClear(GL_COLOR_BUFFER_BIT);
	shaderSSAOBlur->use();
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, ssaoColorBuffer);
	Utils::renderQuad();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	renderDataHandle.mSSAOPassData = {
		ssaoColorBuffer,
		ssaoColorBufferBlur
	};
}
