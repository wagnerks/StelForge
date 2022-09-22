#include "MeshComponent.h"

#include "modelModule/Model.h"


using namespace GameEngine::ComponentsModule;

MeshComponent::MeshComponent(std::vector<ModelModule::MeshHandle> meshData)
	: mMeshData(std::move(meshData))
{
}

const GameEngine::ModelModule::MeshHandle& MeshComponent::getMesh(size_t LOD) {
	if (mMeshData.empty()) {
		static GameEngine::ModelModule::MeshHandle empty;
		return empty;
	}

	if (mMeshData.size() < LOD) {
		return mMeshData.back();
	}

	return mMeshData[LOD];
}

const std::vector<GameEngine::ModelModule::MeshHandle>& MeshComponent::getMeshes() {
	return mMeshData;
}
