#pragma once
#include "ecsModule/EntityBase.h"
#include "modelModule/Model.h"

namespace Engine::EntitiesModule {
	class Model : public ecsModule::Entity<Model> {
	public:
		Model(size_t entId);
		void init(ModelModule::MeshNode& model);
	private:
	};
}
