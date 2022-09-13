#pragma once

#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Mesh.h"


namespace GameEngine {
	namespace ModelModule {
		class Model;
	}
}

namespace GameEngine::ComponentsModule{
	class ModelComponent : public ecsModule::Component<ModelComponent> {
	public:

		void setModel(ModelModule::Model* aModel);
		ModelModule::Model* getModel() const;
	private:
		ModelModule::Model* mModel = nullptr;
	};

	class MeshComponent : public ecsModule::Component<MeshComponent> {
	public:
		MeshComponent(std::vector<ModelModule::MeshHandle> meshData);
		const ModelModule::MeshHandle& getMesh(size_t LOD);
	//private:
		std::vector<ModelModule::MeshHandle> mMeshData;
	};

}

using GameEngine::ComponentsModule::ModelComponent;
using GameEngine::ComponentsModule::MeshComponent;