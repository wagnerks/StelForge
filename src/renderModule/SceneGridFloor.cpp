#include "SceneGridFloor.h"

#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/SystemManager.h"
#include "modelModule/Mesh.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/CameraSystem.h"

using namespace Engine::RenderModule;

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
	floorShader->setVec2("coordShift", { size, size });

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
	auto camera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	floorShader->setMat4("PVM", camera->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix() * camera->getComponent<TransformComponent>()->getViewMatrix() * transform);

	glBindVertexArray(VAO);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	RenderModule::Renderer::drawArrays(GL_TRIANGLES, 6);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glBindVertexArray(0);
}
