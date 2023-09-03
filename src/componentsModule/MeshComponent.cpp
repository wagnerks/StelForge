#include "MeshComponent.h"

#include "modelModule/Model.h"


using namespace Engine::ComponentsModule;

MeshComponent::MeshComponent(std::vector<ModelModule::MeshHandle> meshData)
	: mMeshData(std::move(meshData))
{
}

const Engine::ModelModule::MeshHandle& MeshComponent::getMesh(size_t LOD) {
	if (mMeshData.empty()) {
		static Engine::ModelModule::MeshHandle empty;
		return empty;
	}

	if (mMeshData.size() < LOD) {
		return mMeshData.back();
	}

	return mMeshData[LOD];
}

const std::vector<Engine::ModelModule::MeshHandle>& MeshComponent::getMeshes() {
	return mMeshData;
}
