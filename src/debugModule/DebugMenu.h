#pragma once
#include "DebugInfo.h"

namespace Engine::Debug {
	class DebugMenu {
	public:
		~DebugMenu();
		void draw();

		bool opened = false;

		bool collapsed = true;
		bool debugInfoOpened = true;
		bool shadersDebugOpened = false;
		bool imguiDemo = false;

		DebugInfoType debugInfoType = DebugInfoType::Middle;
	};
}
