#pragma once

#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Mesh.h"


namespace Engine::ComponentsModule{
	class MeshComponent : public ecsModule::Component<MeshComponent> {
	public:
		MeshComponent(std::vector<ModelModule::MeshHandle> meshData);
		const ModelModule::MeshHandle& getMesh(size_t LOD);
		const std::vector<ModelModule::MeshHandle>& getMeshes();
	//private:
		std::vector<ModelModule::MeshHandle> mMeshData;
	};

}

using Engine::ComponentsModule::MeshComponent;