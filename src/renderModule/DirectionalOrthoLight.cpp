#include "DirectionalOrthoLight.h"

#include "imgui.h"
#include "Utils.h"
#include "componentsModule/TransformComponent.h"
#include "glad/glad.h"
#include "assetsModule/shaderModule/ShaderController.h"
using namespace Engine::LightsModule;

DirectionalOrthoLight::DirectionalOrthoLight(size_t entID, int aShadowWidth, int aShadowHeight, float zNear, float zFar) : Light(entID), shadowWidthResolution(aShadowWidth), shadowHeightResolution(aShadowHeight) {

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, shadowWidthResolution, shadowHeightResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	constexpr float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	lightProjection = ProjectionModule::OrthoProjection({ -shadowWidth * 0.5f, -shadowHeight * 0.5f }, { shadowWidth * 0.5f, shadowHeight * 0.5f }, zNear, zFar);
}

DirectionalOrthoLight::~DirectionalOrthoLight() {
	glDeleteFramebuffers(1, &depthMapFBO);
	glDeleteTextures(1, &depthMap);
}

void DirectionalOrthoLight::preDraw() {
	lightPV = lightProjection.getProjectionsMatrix() * getComponent<TransformComponent>()->getViewMatrix();

	auto simpleDepth = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/depth.vs", "shaders/depth.fs");
	simpleDepth->use();
	simpleDepth->setMat4("lightSpaceMatrix", lightPV);


	glViewport(0, 0, shadowWidthResolution, shadowHeightResolution);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glCullFace(GL_FRONT);
}

void DirectionalOrthoLight::postDraw() {
	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, RenderModule::Renderer::SCR_WIDTH, RenderModule::Renderer::SCR_HEIGHT);
}

unsigned DirectionalOrthoLight::getDepthMapTexture() const {
	return depthMap;
}

const glm::mat4& DirectionalOrthoLight::getProjectionViewMatrix() const {
	return lightPV;
}

const Engine::ProjectionModule::OrthoProjection& DirectionalOrthoLight::getProjection() {
	return lightProjection;
}
