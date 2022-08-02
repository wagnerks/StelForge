#pragma once
#include "Component.h"

namespace GameEngine {
	namespace ModelModule {
		class Model;
	}
}

namespace GameEngine::ComponentsModule{
	class ModelComponent : public Component {
	public:
		ModelComponent(ComponentHolder* holder) : Component(holder){}

		void updateComponent() override {};
		void draw();
		void setModel(ModelModule::Model* aModel);
		ModelModule::Model* getModel() const;
	private:
		ModelModule::Model* model = nullptr;
	};

}

using GameEngine::ComponentsModule::ModelComponent;