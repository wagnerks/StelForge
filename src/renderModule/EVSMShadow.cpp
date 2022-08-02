#include "EVSMShadow.h"

#include <ext/matrix_clip_space.hpp>

#include "imgui.h"
#include "TextureHandler.h"
#include "Utils.h"
#include "componentsModule/TransformComponent.h"
#include "glad/glad.h"
#include "logsModule/logger.h"
#include "shaderModule/ShaderController.h"

EVSMShadow::EVSMShadow() {
	shadowWidth = 1024;
	shadowHeight = 1024;

	glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
   
    // position color buffer
    glGenTextures(1, &depthMap);
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, depthMap);
    //glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadowWidth, shadowHeight, 0, GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 16.f);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthMap, 0);

    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0};

    glDrawBuffers(1, attachments);
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	    GameEngine::LogsModule::Logger::LOG_ERROR("Framebuffer not complete!");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	auto tc = getComponent<TransformComponent>();
	tc->setPos({0.f,4.f,0.f});
	tc->setRotate({171.f,44.f,0.f});
	tc->setScale({5.f,5.f,1.f});
}

EVSMShadow::~EVSMShadow() {
	glDeleteFramebuffers(1, &depthMapFBO);
	glDeleteTextures(1, &depthMap);
}

void EVSMShadow::preDraw() {
	ImGui::Begin("evsm debug");
	float exp[2] = {exponents.x, exponents.y};
	if (ImGui::DragFloat2("exponents", exp)) {
		exponents = {exp[0], exp[1]};
	}
	ImGui::DragFloat("u_LightBleedReduction", &u_LightBleedReduction, 0.001f, 0.f,1.f);
	ImGui::DragFloat("u_VarianceMinLimit", &u_VarianceMinLimit, 0.001f);
	ImGui::DragFloat("u_ShadowBias", &u_ShadowBias, 0.001f);
	ImGui::DragInt("u_EVSMMode", &u_EVSMMode, 1, 0, 1);
	ImGui::DragInt("gauss", &gaussAlgorithm,1,0,4);
	float blur[2] = {blurAmount.x, blurAmount.y};
	if (ImGui::DragFloat2("blurAmount", blur, 0.1f)) {
		blurAmount = {blur[0], blur[1]};
	}
	ImGui::End();
	auto tc = getComponent<TransformComponent>();
	tc->reloadTransform();
	
	glm::mat4 lightProjection = glm::ortho(-tc->getScale().x * 1.0f, tc->getScale().x * 1.0f, -tc->getScale().y * 1.0f, tc->getScale().y * 1.0f, nearPlane, farPlane);
    lightSpaceMatrix = lightProjection * tc->getViewMatrix();

	auto simpleDepth = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/evsm_1st_pass.vert", "shaders/evsm_1st_pass.frag");
	simpleDepth->use();
	simpleDepth->setMat4("lightSpaceMatrix", lightSpaceMatrix);
	simpleDepth->setVec2("u_Exponents", exponents);
	glViewport(0, 0, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight));
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	//glActiveTexture(GL_TEXTURE0);
	//glCullFace(GL_FRONT);
}
void EVSMShadow::postDraw() {
	glClear(GL_DEPTH_BUFFER_BIT);
	GameEngine::ShaderModule::ShaderBase* blurShader = nullptr;
	switch(gaussAlgorithm) {
		case 0:{
			blurShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/basic_blur.vert", "shaders/gblur_3x3.frag");
			break;
		}
		case 1:{
			blurShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/basic_blur.vert", "shaders/gblur_5x5_separated.frag");
			break;
		}
		case 2:{
			blurShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/basic_blur.vert", "shaders/gblur_5x5.frag");
			break;
		}
		case 3:{
			blurShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/basic_blur.vert", "shaders/gblur_7x7_separated.frag");
			break;
		}
		case 4:{
			blurShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/basic_blur.vert", "shaders/gblur_9x9_separated.frag");
			break;
		}
	}

	if (blurShader) {
		blurShader->use();
		GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, depthMap);

		blurShader->setInt("u_InputTexture", 0);
		blurShader->setVec2("u_TexelSize", {(1.f / static_cast<float>(shadowWidth)) * blurAmount.x,(1.f / static_cast<float>(shadowHeight) * blurAmount.y)});
		GameEngine::RenderModule::Utils::renderQuad();
	}
	
	//glCullFace(GL_BACK); 
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, GameEngine::RenderModule::Renderer::SCR_WIDTH, GameEngine::RenderModule::Renderer::SCR_HEIGHT);
}
