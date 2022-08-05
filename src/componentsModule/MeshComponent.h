#pragma once

#include "ecsModule/ComponentBase.h"
#include "modelModule/Mesh.h"

namespace GameEngine::ComponentsModule{
	class MeshComponent : public ecsModule::Component<MeshComponent> {
	public:

		void setMesh(ModelModule::Mesh* aMesh);
		ModelModule::Mesh* getMesh();
	private:
		ModelModule::Mesh* mesh = nullptr;
	};
}

using GameEngine::ComponentsModule::MeshComponent;
