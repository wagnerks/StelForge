#pragma once
#include "systemsModule/SystemBase.h"
namespace Engine::SystemsModule {
	class WorldTimeSystem : public ecss::System {
	public:
		void update(float dt) override;

		float getWorldTime() const { return mWorldTime; }
	private:
		float mWorldTime = 0.f;
	};
}
