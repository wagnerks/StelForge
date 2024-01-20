#pragma once
#include "systemsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class ActionSystem : public ecss::System {
	public:
		void update(float dt) override;
	};
}
