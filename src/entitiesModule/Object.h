#pragma once
#include "ecsModule/EntityBase.h"

namespace Engine::EntitiesModule {
	class Object : public ecsModule::Entity<Object> {
	public:
		Object(size_t entId);
	};
}
