#include "SceneGridFloor.h"

#include "core/Camera.h"
#include "core/Engine.h"
#include "modelModule/Mesh.h"
#include "shaderModule/ShaderController.h"

using namespace GameEngine::RenderModule;

SceneGridFloor::SceneGridFloor(float size) : size(size) {}
SceneGridFloor::~SceneGridFloor() {
	if (VAO != -1) {
		glDeleteVertexArrays(1, &VAO);
	}
	if (VBO != -1) {
		glDeleteBuffers(1, &VBO);
	}

	SHADER_CONTROLLER->deleteShader(floorShader);
}

void SceneGridFloor::init() {
	floorShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/floorGrid.vs", "shaders/floorGrid.fs");
	if (!floorShader) {
		return;
	}

	floorShader->use();
	floorShader->setVec2("coordShift", {size, size});

	float vertices[] = {
		size, 0.f, -size, //far right
		-size, 0.f, -size,//far left
		-size, 0.f, size, //near left
		size, 0.f, -size, //far right
		-size, 0.f, size,//near left
		size, 0.f, size, //near right
    };

	
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

}
void SceneGridFloor::draw() {
	if (VAO == -1) {
		return;
	}
	floorShader->use();

	floorShader->setMat4("PVM", Engine::getInstance()->getCamera()->getProjectionsMatrix()  * GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix() * transform);

	glBindVertexArray(VAO);
	glDisable(GL_CULL_FACE);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	GameEngine::RenderModule::Renderer::drawCallsCount++;
	GameEngine::RenderModule::Renderer::drawVerticesCount += 6;
	glEnable(GL_CULL_FACE);

	glBindVertexArray(0);
}
