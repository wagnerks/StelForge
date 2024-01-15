#include "Skybox.h"

#include "assetsModule/TextureHandler.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/SystemManager.h"

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
		//AssetsModule::TextureHandler::instance()->deleteTexture(); TODO
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
	skyboxShader->setMat4("projection", ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getProjection().getProjectionsMatrix());

	cubemapTex = AssetsModule::TextureHandler::instance()->loadCubemapTexture(skyboxPath)->mId;
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

	auto view = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera())->getViewMatrix();;
	skyboxShader->setMat4("view", Math::Mat4(Math::Mat3{view}));

	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content

	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE16, GL_TEXTURE_CUBE_MAP, cubemapTex);

	glBindVertexArray(VAO);
	RenderModule::Renderer::drawArrays(GL_TRIANGLES, 36);

	glDepthFunc(GL_LESS);
	glBindVertexArray(0);
}
