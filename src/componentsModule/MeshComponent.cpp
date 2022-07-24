#include "MeshComponent.h"

#include "ShaderComponent.h"
#include "TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "shaderModule/ShaderController.h"

using namespace GameEngine::ComponentsModule;

void MeshComponent::setMesh(ModelModule::Mesh* aMesh) {
	mesh = aMesh;
}

GameEngine::ModelModule::Mesh* MeshComponent::getMesh() {
	return mesh;
}

void MeshComponent::draw() {
	if (!mesh) {
		return;
	}

	auto shaderComp = owner->getComponent<ShaderComponent>(false);
	auto transformComp = owner->getComponent<TransformComponent>(false);
	auto shader = shaderComp ? shaderComp->getShader() : SHADER_CONTROLLER->defaultShader;

	auto projection = Engine::getInstance()->getCamera()->getProjectionsMatrix();

	// camera/view transformation
	; 
	auto view =Engine::getInstance()->getCamera()->GetViewMatrix();
	shader->use();
	if (transformComp) {
		transformComp->reloadTransform();
		shader->setMat4("model", transformComp->getTransform());
		shader->setMat4("PVM", projection * view * transformComp->getTransform());
		shader->setMat4("projection", projection);
		shader->setMat4("view",  view);
		shader->setMat4("skyView", glm::mat4(glm::mat3(view)));
		shader->setMat4("PVM", projection * view * transformComp->getTransform());
		
        shader->setVec3("cameraPos", Engine::getInstance()->getCamera()->Position);
		shader->setInt("skybox", 0);
	}
	
	

	mesh->draw(shader);
}
