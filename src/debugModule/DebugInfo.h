#pragma once

namespace Engine::Debug {

	enum class DebugInfoType {
		Small,
		Middle,
		Big
	};

	class DebugInfo {
	public:
		static void drawInfo(DebugInfoType type);
		inline static bool opened = true;
	};
}

