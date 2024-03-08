#pragma once
#include "debugModule/DebugMenu.h"
#include "debugModule/imguiDecorator.h"

namespace SFE::CoreModule {
	class Core {
	public:
		Core() = default;
		~Core();

		void update(float dt);
		void init();
	private:
		Debug::DebugMenu mDebugMenu;
		Debug::ImGuiDecorator mDecorator;
	};
}

