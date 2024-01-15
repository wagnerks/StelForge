#include "SSAOPass.h"

#include <random>
#include <gtc/random.hpp>

#include "imgui.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/Benchmark.h"
#include "logsModule/logger.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"

using namespace Engine::RenderModule::RenderPasses;

float lerp(float a, float b, float f) {
	return a + f * (b - a);
}

void SSAOPass::init() {
	glGenFramebuffers(1, &mData.mSsaoFbo);
	glGenFramebuffers(1, &mData.mSsaoBlurFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mSsaoFbo);

	// SSAO color buffer
	glGenTextures(1, &mData.mSsaoColorBuffer);
	glBindTexture(GL_TEXTURE_2D, mData.mSsaoColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.mSsaoColorBuffer, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_FATAL(false, "SSAO Framebuffer not complete!");
	}
	// and blur stage
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mSsaoBlurFbo);
	glGenTextures(1, &mData.mSsaoColorBufferBlur);
	glBindTexture(GL_TEXTURE_2D, mData.mSsaoColorBufferBlur);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.mSsaoColorBufferBlur, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_FATAL(false, "SSAO Blur Framebuffer not complete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// generate sample kernel
	// ----------------------

	std::random_device rd;
	std::mt19937 generator(rd());

	// Define the distribution for the range -5.0f to 15.0f
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distribution2(0.0f, 1.0f);

	for (unsigned int i = 0; i < 64; ++i) {
		Math::Vec3 sample(distribution(generator), distribution(generator), distribution2(generator));
		sample = Math::normalize(Math::Vec3{ sample.x, sample.y, sample.z });
		sample *= distribution(generator);
		float scale = float(i) / 64.0f;

		// scale samples s.t. they're more aligned to center of kernel
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		mData.mSsaoKernel.push_back(sample);
	}

	// generate noise texture
	// ----------------------
	std::vector<Math::Vec3> ssaoNoise;
	for (unsigned int i = 0; i < 16; i++) {
		Math::Vec3 noise(distribution(generator), distribution(generator), 0.0f);
		// rotate around z-axis (in tangent space)
		ssaoNoise.push_back(noise);
	}

	glGenTextures(1, &mData.mNoiseTexture);
	glBindTexture(GL_TEXTURE_2D, mData.mNoiseTexture);
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

	shaderSSAO->setInt("kernelSize", mData.mKernelSize);
	shaderSSAO->setFloat("radius", mData.mRadius);
	shaderSSAO->setFloat("bias", mData.mBias);
	shaderSSAO->setVec2("noiseScale", Math::Vec2{static_cast<float>(Renderer::SCR_WIDTH) / 4.f, static_cast<float>(Renderer::SCR_WIDTH) / 4.f});
	for (unsigned int i = 0; i < 64; ++i) {
		shaderSSAO->setVec3(("samples[" + std::to_string(i) + "]").c_str(), mData.mSsaoKernel[i]);
	}

	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");
	shaderSSAOBlur->use();
	shaderSSAOBlur->setInt("ssaoInput", 0);

	float facL = -1.f / (2.f * mData.sigmaL * mData.sigmaL);
	shaderSSAOBlur->setFloat("sigmaL", mData.sigmaL);
	shaderSSAOBlur->setFloat("facL", facL);

	float facS = -1.f / (2.f * mData.sigmaS * mData.sigmaS);
	shaderSSAOBlur->setFloat("sigmaS", mData.sigmaS);
	shaderSSAOBlur->setFloat("facS", facS);

}

void SSAOPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	FUNCTION_BENCHMARK
	auto shaderSSAO = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao.fs");
	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");

	if (ImGui::Begin("SSAO")) {
		shaderSSAO->use();
		if (ImGui::DragInt("kernelSize", &mData.mKernelSize)) {
			shaderSSAO->setInt("kernelSize", mData.mKernelSize);
		}
		if (ImGui::DragFloat("radius", &mData.mRadius, 0.01f)) {
			shaderSSAO->setFloat("radius", mData.mRadius);
		}
		if (ImGui::DragFloat("bias", &mData.mBias, 0.001f)) {
			shaderSSAO->setFloat("BIAS", mData.mBias);
		}
		if (ImGui::DragFloat("intencity", &mData.intencity, 0.1f)) {
			shaderSSAO->setFloat("INTENSITY", mData.intencity);
		}

		if (ImGui::DragFloat("scale", &mData.scale, 0.1f)) {
			shaderSSAO->setFloat("SCALE", mData.scale);
		}

		if (ImGui::DragFloat("sample_rad", &mData.sample_rad, 0.1f)) {
			shaderSSAO->setFloat("SAMPLE_RAD", mData.sample_rad);
		}

		if (ImGui::DragFloat("max_distance", &mData.max_distance, 0.1f)) {
			shaderSSAO->setFloat("MAX_DISTANCE", mData.max_distance);
		}
		if (ImGui::DragInt("samples", &mData.samples)) {
			shaderSSAO->setInt("SAMPLES", mData.samples);
		}

		if (ImGui::DragFloat("sigmaS", &mData.sigmaS, 0.01f, 0.000001f)) {
			shaderSSAOBlur->use();
			float facS = -1.f / (2.f * mData.sigmaS * mData.sigmaS);

			shaderSSAOBlur->setFloat("sigmaS", mData.sigmaS);
			shaderSSAOBlur->setFloat("facS", facS);
		}
		if (ImGui::DragFloat("sigmaL", &mData.sigmaL, 0.01f, 0.000001f)) {
			shaderSSAOBlur->use();
			float facL = -1.f / (2.f * mData.sigmaL * mData.sigmaL);
			shaderSSAOBlur->setFloat("sigmaL", mData.sigmaL);
			shaderSSAOBlur->setFloat("facL", facL);
		}
	}
	ImGui::End();

	glBindFramebuffer(GL_FRAMEBUFFER, mData.mSsaoFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	shaderSSAO->use();
	shaderSSAO->setMat4("projection", renderDataHandle.current.projection);

	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gViewPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, mData.mNoiseTexture);
	Utils::renderQuad();


	glBindFramebuffer(GL_FRAMEBUFFER, mData.mSsaoBlurFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	shaderSSAOBlur->use();
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.mSsaoColorBuffer);
	Utils::renderQuad();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	renderDataHandle.mSSAOPassData = mData;
}
