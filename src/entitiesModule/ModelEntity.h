#pragma once
#include "ecsModule/EntityBase.h"

namespace GameEngine::EntitiesModule {
	class Model : public ecsModule::Entity<Model> {
	public:
		Model(size_t entId);
	private:
	};
}
