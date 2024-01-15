#include "SceneGridFloor.h"

#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"

using namespace Engine::RenderModule;

SceneGridFloor::SceneGridFloor() {
	init();
}
SceneGridFloor::~SceneGridFloor() {
	if (VAO != -1) {
		glDeleteVertexArrays(1, &VAO);
	}
	if (VBO != -1) {
		glDeleteBuffers(1, &VBO);
	}
	
	SHADER_CONTROLLER->deleteShader(floorShaderHash);
}

void SceneGridFloor::init() {
	floorShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/floorGrid.vs", "shaders/floorGrid.fs");
	if (!floorShader) {
		return;
	}

	floorShaderHash = floorShader->getHash();

	floorShader->use();
	transform = Math::translate(transform, Math::Vec3(0.f));

	float vertices[] = {
		 1.0, 0.f, -1.0, //far right
		-1.0, 0.f, -1.0,//far left
		-1.0, 0.f,  1.0, //near left

		 1.0, 0.f, -1.0, //far right
		-1.0, 0.f,  1.0,//near left
		 1.0, 0.f,  1.0, //near right
	};


	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 18, &vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneGridFloor::draw() {
	if (VAO == -1) {
		return;
	}
	floorShader->use();
	auto& renderData = ECSHandler::getSystem<Engine::SystemsModule::RenderSystem>()->getRenderData();
	auto camera = ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);
	//cameraTransform->reloadTransform();
	
	floorShader->setMat4("PVM", renderData.current.PV * transform);
	floorShader->setMat4("model", transform);
	
	floorShader->setVec3("cameraPos", Math::Vec3{renderData.mCameraPos});

	floorShader->setFloat("far", cameraComp->getProjection().getFar());
	floorShader->setFloat("near", cameraComp->getProjection().getNear());


	glBindVertexArray(VAO);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	RenderModule::Renderer::drawArrays(GL_TRIANGLES, 6);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glBindVertexArray(0);
}
