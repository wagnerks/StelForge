#pragma once
#include "ecsModule/SystemBase.h"

namespace Engine {
	namespace ComponentsModule {
		class TransformComponent;
	}
}

namespace Engine::SystemsModule {
	class TransformSystem : public ecsModule::System<TransformSystem> {
	public:
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override{}
	};
}