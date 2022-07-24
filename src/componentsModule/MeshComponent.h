#pragma once
#include "Component.h"
#include "ComponentHolder.h"
#include "modelModule/Mesh.h"

namespace GameEngine::ComponentsModule{
	class MeshComponent : public Component {
	public:
		MeshComponent(ComponentHolder* holder) : Component(holder){}
		void updateComponent() override{};

		void setMesh(ModelModule::Mesh* aMesh);
		ModelModule::Mesh* getMesh();
		void draw();
	private:
		ModelModule::Mesh* mesh = nullptr;
		
	};
}
