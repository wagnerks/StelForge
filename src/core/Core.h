#pragma once
#include "debugModule/DebugMenu.h"

namespace Engine::CoreModule {
	class Core {
	public:
		Core();
		~Core();

		void update(float dt);
		void init();
	private:
		Debug::DebugMenu mDebugMenu;
	};
}

