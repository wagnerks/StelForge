#pragma once

namespace GameEngine::Debug {
	class DebugMenu {
	public:
		void draw();

		bool opened = false;

		bool debugInfoOpened = true;
		bool shadersDebugOpened = false;
	};
}