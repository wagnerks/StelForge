#pragma once
#include "mathModule/Forward.h"

namespace SFE::ComponentsModule {
	class ActionComponent {
	public:
		float progress = 0.f;
		float time = 0.f;
		bool loop = false;

		Math::Vec3 initialPos = {};
	};
}