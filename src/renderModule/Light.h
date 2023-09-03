#pragma once
#include "ecsModule/EntityBase.h"


namespace Engine::LightsModule {
	class Light : public ecsModule::Entity<Light> {
	public:
		Light(size_t entID) : Entity<Light>(entID) {};
	};
}
