#pragma once
#include "mathModule/Forward.h"

namespace Engine::ComponentsModule {
	class ActionComponent {
	public:
		float progress = 0.f;
		float time = 0.f;
		bool loop = false;

		Math::Vec3 initialPos = {};
	};
}