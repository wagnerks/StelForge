#include "ModelComponent.h"

#include "modelModule/Model.h"


using namespace GameEngine::ComponentsModule;

void ModelComponent::setModel(ModelModule::Model* aModel) {
	model = aModel;
}

GameEngine::ModelModule::Model* ModelComponent::getModel() const {
	return model;
}
