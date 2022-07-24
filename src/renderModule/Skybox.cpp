#include "Skybox.h"

#include "TextureHandler.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "modelModule/Mesh.h"
#include "shaderModule/ShaderController.h"

using namespace GameEngine::RenderModule;

Skybox::Skybox(std::string_view path) : skyboxPath(path) {}

void Skybox::init() {
	skyboxShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/skybox.vs", "shaders/skybox.fs");
	skyboxShader->setInt("skybox", 16);
		
	cubemapTex = TextureHandler::getInstance()->loader.loadCubemapTexture(skyboxPath);
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

	unsigned VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void Skybox::draw() {
	skyboxShader->use();

	auto view = Engine::getInstance()->getCamera()->GetViewMatrix();

	skyboxShader->setMat4("projection", Engine::getInstance()->getCamera()->getProjectionsMatrix());
	skyboxShader->setMat4("view", glm::mat4(glm::mat3(view)));

	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content

	TextureHandler::getInstance()->bindTexture(GL_TEXTURE16, GL_TEXTURE_CUBE_MAP, cubemapTex);
   
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	RenderModule::Renderer::drawCallsCount++;
	RenderModule::Renderer::drawVerticesCount += 36;

	glDepthFunc(GL_LESS);
	glBindVertexArray(0);
}
