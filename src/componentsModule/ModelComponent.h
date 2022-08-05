#pragma once

#include "ecsModule/ComponentBase.h"


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
		ModelModule::Model* model = nullptr;
	};

}

using GameEngine::ComponentsModule::ModelComponent;