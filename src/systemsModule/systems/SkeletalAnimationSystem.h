#pragma once
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class SkeletalAnimationSystem : public ecss::System {
	public:
		void update(float dt) override;
	};
}
