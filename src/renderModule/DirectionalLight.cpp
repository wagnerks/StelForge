#include "DirectionalLight.h"

#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>

#include "imgui.h"
#include "Utils.h"
#include "componentsModule/TransformComponent.h"
#include "glad/glad.h"
#include "shaderModule/ShaderController.h"
using namespace GameEngine::LightsModule;

DirectionalLight::DirectionalLight(size_t aShadowWidth, size_t aShadowHeight, float aNearPlane, float aFarPlane) : shadowWidth(aShadowWidth), shadowHeight(aShadowHeight), nearPlane(aNearPlane), farPlane(aFarPlane) {
	glGenFramebuffers(1, &depthMapFBO);

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);  
}

DirectionalLight::~DirectionalLight() {
	glDeleteFramebuffers(1, &depthMapFBO);
	glDeleteTextures(1, &depthMap);
}

void DirectionalLight::preDraw() {
	ImGui::Begin("lightDebug");
	ImGui::DragFloat("nearPlane", &nearPlane,0.1f);
	ImGui::DragFloat("farPlane", &farPlane,0.1f);
	ImGui::End();

	auto tc = getComponent<TransformComponent>();
	tc->setWithView(true);
	tc->reloadTransform();
	
	glm::mat4 lightProjection = glm::ortho(-tc->getScale().x * 1.0f, tc->getScale().x * 1.0f, -tc->getScale().y * 1.0f, tc->getScale().y * 1.0f, nearPlane, farPlane);
    lightSpaceMatrix = lightProjection * tc->getViewMatrix();

	auto simpleDepth = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/depth.vs", "shaders/depth.fs");
	simpleDepth->use();
	simpleDepth->setMat4("lightSpaceMatrix", lightSpaceMatrix);

	glViewport(0, 0, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight));
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);
	glCullFace(GL_FRONT);
}

void DirectionalLight::postDraw() {
	glCullFace(GL_BACK); 
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, RenderModule::Renderer::SCR_WIDTH, RenderModule::Renderer::SCR_HEIGHT);

}

unsigned DirectionalLight::getDepthMapTexture() {
	return depthMap;
}

const glm::mat4& DirectionalLight::getLightSpaceProjection() {
	return lightSpaceMatrix;
}

float DirectionalLight::getNearPlane() {
	return nearPlane;
}
float DirectionalLight::getFarPlane() {
	return farPlane;
}
