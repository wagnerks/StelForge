#include "Skybox.h"

#include "assetsModule/TextureHandler.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/SystemManager.h"
#include "assetsModule/modelModule/Mesh.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/CameraSystem.h"

using namespace Engine::RenderModule;

Skybox::Skybox(std::string_view path) : skyboxPath(path) {}

Skybox::~Skybox() {
	if (VAO != -1) {
		glDeleteVertexArrays(1, &VAO);
	}
	if (VBO != -1) {
		glDeleteBuffers(1, &VBO);
	}
	if (cubemapTex != -1) {
		//TextureHandler::getInstance()->deleteTexture(); TODO
		glDeleteTextures(1, &cubemapTex);
	}

	SHADER_CONTROLLER->deleteShader(skyboxShader);
}

void Skybox::init() {
	skyboxShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/skybox.vs", "shaders/skybox.fs");
	if (!skyboxShader) {
		return;
	}

	skyboxShader->use();
	skyboxShader->setInt("skybox", 16);
	skyboxShader->setMat4("projection", ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix());

	cubemapTex = AssetsModule::TextureHandler::instance()->mLoader.loadCubemapTexture(skyboxPath).mId;
	if (cubemapTex == 0) {
		assert(false && "can't load skybox texture");
		return;
	}

	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};


	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void Skybox::draw() {
	if (VAO == -1) {
		return;
	}
	skyboxShader->use();

	auto view = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<TransformComponent>()->getViewMatrix();;
	skyboxShader->setMat4("view", glm::mat4(glm::mat3(view)));

	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content

	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE16, GL_TEXTURE_CUBE_MAP, cubemapTex);

	glBindVertexArray(VAO);
	RenderModule::Renderer::drawArrays(GL_TRIANGLES, 36);

	glDepthFunc(GL_LESS);
	glBindVertexArray(0);
}
