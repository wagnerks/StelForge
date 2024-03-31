#include "SSAOPass.h"

#include <random>
#include <gtc/random.hpp>

#include "imgui.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/Benchmark.h"
#include "logsModule/logger.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemsPriority.h"

using namespace SFE::Render::RenderPasses;

float lerp(float a, float b, float f) {
	return a + f * (b - a);
}

void SSAOPass::init() {
	
	mData.mSsaoFbo.bind();

	// SSAO color buffer
	mData.mSsaoColorBuffer.width = Engine::instance()->getWindow()->getScreenData().renderW;
	mData.mSsaoColorBuffer.height = Engine::instance()->getWindow()->getScreenData().renderH;
	mData.mSsaoColorBuffer.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.mSsaoColorBuffer.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.mSsaoColorBuffer.pixelFormat = GLW::R8;
	mData.mSsaoColorBuffer.textureFormat = GLW::RED;
	mData.mSsaoColorBuffer.pixelType = GLW::FLOAT;
	mData.mSsaoColorBuffer.create();

	mData.mSsaoColorBufferBlur.width = Engine::instance()->getWindow()->getScreenData().renderW;
	mData.mSsaoColorBufferBlur.height = Engine::instance()->getWindow()->getScreenData().renderH;
	mData.mSsaoColorBufferBlur.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.mSsaoColorBufferBlur.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.mSsaoColorBufferBlur.pixelFormat = GLW::R8;
	mData.mSsaoColorBufferBlur.textureFormat = GLW::RED;
	mData.mSsaoColorBufferBlur.pixelType = GLW::FLOAT;
	mData.mSsaoColorBufferBlur.create();
	
	mData.mSsaoFbo.addAttachmentTexture(0, &mData.mSsaoColorBuffer);
	mData.mSsaoFbo.finalize();

	// and blur stage
	mData.mSsaoBlurFbo.bind();
	mData.mSsaoBlurFbo.addAttachmentTexture(0, &mData.mSsaoColorBufferBlur);
	mData.mSsaoBlurFbo.finalize();

	GLW::Framebuffer::bindDefaultFramebuffer();
	GLW::Framebuffer::bindDefaultFramebuffer();
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
	mData.mNoiseTexture.width = 4;
	mData.mNoiseTexture.height = 4;
	mData.mNoiseTexture.parameters.minFilter = GLW::TextureMinFilter::NEAREST;
	mData.mNoiseTexture.parameters.magFilter = GLW::TextureMagFilter::NEAREST;
	mData.mNoiseTexture.pixelFormat = GLW::RGB32F;
	mData.mNoiseTexture.textureFormat = GLW::RGB;
	mData.mNoiseTexture.pixelType = GLW::FLOAT;
	mData.mNoiseTexture.create(ssaoNoise.data());

	auto shaderSSAO = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao.fs");
	shaderSSAO->use();
	shaderSSAO->setUniform("gPosition", 0);
	shaderSSAO->setUniform("gNormal", 1);
	shaderSSAO->setUniform("texNoise", 2);

	shaderSSAO->setUniform("kernelSize", mData.mKernelSize);
	shaderSSAO->setUniform("radius", mData.mRadius);
	shaderSSAO->setUniform("bias", mData.mBias);
	shaderSSAO->setUniform("noiseScale", Math::Vec2{static_cast<float>(Engine::instance()->getWindow()->getScreenData().renderW) / 4.f, static_cast<float>(Engine::instance()->getWindow()->getScreenData().renderW) / 4.f});
	for (unsigned int i = 0; i < 64; ++i) {
		shaderSSAO->setUniform(("samples[" + std::to_string(i) + "]").c_str(), mData.mSsaoKernel[i]);
	}

	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");
	shaderSSAOBlur->use();
	shaderSSAOBlur->setUniform("ssaoInput", 0);

	float facL = -1.f / (2.f * mData.sigmaL * mData.sigmaL);
	shaderSSAOBlur->setUniform("sigmaL", mData.sigmaL);
	shaderSSAOBlur->setUniform("facL", facL);

	float facS = -1.f / (2.f * mData.sigmaS * mData.sigmaS);
	shaderSSAOBlur->setUniform("sigmaS", mData.sigmaS);
	shaderSSAOBlur->setUniform("facS", facS);

}

void SSAOPass::render(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK
	auto shaderSSAO = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao.fs");
	auto shaderSSAOBlur = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/ssao.vs", "shaders/ssao_blur.fs");
	{
		/*if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Debug")) {
				if (ImGui::BeginMenu("Systems debug")) {
					ImGui::Checkbox("SSAO", &ssaoDebugWindow);
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndMainMenuBar();*/
	}

	if (ssaoDebugWindow) {
		if (ImGui::Begin("SSAO", &ssaoDebugWindow)) {
			shaderSSAO->use();
			if (ImGui::DragInt("kernelSize", &mData.mKernelSize)) {
				shaderSSAO->setUniform("kernelSize", mData.mKernelSize);
			}
			if (ImGui::DragFloat("radius", &mData.mRadius, 0.01f)) {
				shaderSSAO->setUniform("radius", mData.mRadius);
			}
			if (ImGui::DragFloat("bias", &mData.mBias, 0.001f)) {
				shaderSSAO->setUniform("BIAS", mData.mBias);
			}
			if (ImGui::DragFloat("intencity", &mData.intencity, 0.1f)) {
				shaderSSAO->setUniform("INTENSITY", mData.intencity);
			}

			if (ImGui::DragFloat("scale", &mData.scale, 0.1f)) {
				shaderSSAO->setUniform("SCALE", mData.scale);
			}

			if (ImGui::DragFloat("sample_rad", &mData.sample_rad, 0.1f)) {
				shaderSSAO->setUniform("SAMPLE_RAD", mData.sample_rad);
			}

			if (ImGui::DragFloat("max_distance", &mData.max_distance, 0.1f)) {
				shaderSSAO->setUniform("MAX_DISTANCE", mData.max_distance);
			}
			if (ImGui::DragInt("samples", &mData.samples)) {
				shaderSSAO->setUniform("SAMPLES", mData.samples);
			}

			if (ImGui::DragFloat("sigmaS", &mData.sigmaS, 0.01f, 0.000001f)) {
				shaderSSAOBlur->use();
				float facS = -1.f / (2.f * mData.sigmaS * mData.sigmaS);

				shaderSSAOBlur->setUniform("sigmaS", mData.sigmaS);
				shaderSSAOBlur->setUniform("facS", facS);
			}
			if (ImGui::DragFloat("sigmaL", &mData.sigmaL, 0.01f, 0.000001f)) {
				shaderSSAOBlur->use();
				float facL = -1.f / (2.f * mData.sigmaL * mData.sigmaL);
				shaderSSAOBlur->setUniform("sigmaL", mData.sigmaL);
				shaderSSAOBlur->setUniform("facL", facL);
			}
		}
		ImGui::End();
	}
	
	mData.mSsaoFbo.bind();
	GLW::clear(GLW::ColorBit::COLOR);
	shaderSSAO->use();
	shaderSSAO->setUniform("projection", renderDataHandle.current.projection);

	GLW::bindTextureToSlot(0, &renderDataHandle.mGeometryPassData->viewPositionBuffer);
	GLW::bindTextureToSlot(1, &renderDataHandle.mGeometryPassData->normalBuffer);
	GLW::bindTextureToSlot(2, &mData.mNoiseTexture);
	Utils::renderQuad();
	

	mData.mSsaoBlurFbo.bind();
	GLW::clear(GLW::ColorBit::COLOR);
	shaderSSAOBlur->use();
	GLW::bindTextureToSlot(0, &mData.mSsaoColorBuffer);
	Utils::renderQuad();

	mData.mSsaoBlurFbo.bindDefaultFramebuffer();
	GLW::Framebuffer::bindDefaultFramebuffer();
	renderDataHandle.mSSAOPassData = &mData;
}
