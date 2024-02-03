#pragma once
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class ActionSystem : public ecss::System {
	public:
		void update(float dt) override;
	};
}
