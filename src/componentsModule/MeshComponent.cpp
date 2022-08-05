#include "MeshComponent.h"

using namespace GameEngine::ComponentsModule;

void MeshComponent::setMesh(ModelModule::Mesh* aMesh) {
	mesh = aMesh;
}

GameEngine::ModelModule::Mesh* MeshComponent::getMesh() {
	return mesh;
}
