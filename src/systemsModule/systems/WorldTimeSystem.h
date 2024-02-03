#pragma once
#include "systemsModule/SystemBase.h"
namespace SFE::SystemsModule {
	class WorldTimeSystem : public ecss::System {
	public:
		void update(float dt) override;

		float getWorldTime() const { return mWorldTime; }
	private:
		float mWorldTime = 0.f;
	};
}
