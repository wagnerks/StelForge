#include "ModelComponent.h"

#include "ComponentHolder.h"
#include "ShaderComponent.h"
#include "TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "modelModule/Model.h"
#include "shaderModule/ShaderController.h"


using namespace GameEngine::ComponentsModule;

void ModelComponent::draw() {
	auto shaderComp = owner->getComponent<ShaderComponent>(false);
	auto transformComp = owner->getComponent<TransformComponent>(false);
	auto shader = shaderComp ? shaderComp->getShader() : SHADER_CONTROLLER->defaultShader;

	auto projection = Engine::getInstance()->getCamera()->getProjectionsMatrix();

	// camera/view transformation
	auto view = Engine::getInstance()->getCamera()->GetViewMatrix();
	transformComp->reloadTransform();
	shader->use();
	shader->setMat4("model", transformComp->getTransform());
	shader->setMat4("PVM", projection * view * transformComp->getTransform());
	shader->setMat4("projection", projection);
	shader->setMat4("view",  view);
	shader->setVec3("cameraPos", Engine::getInstance()->getCamera()->Position);
	shader->setFloat("time", glfwGetTime());  
	model->draw(shader);
}

void ModelComponent::setModel(ModelModule::Model* aModel) {
	model = aModel;
}
