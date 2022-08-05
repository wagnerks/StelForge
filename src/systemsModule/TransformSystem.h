#pragma once
#include "ecsModule/SystemBase.h"

namespace GameEngine {
	namespace NodeModule {
		class Node;
	}
}

namespace GameEngine {
	namespace ComponentsModule {
		class TransformComponent;
	}
}

namespace GameEngine::SystemsModule {
	class TransformSystem : public ecsModule::System<TransformSystem> {
	public:
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override{}
	};
}