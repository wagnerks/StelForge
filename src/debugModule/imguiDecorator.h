#pragma once

struct GLFWwindow;

namespace Engine::Debug {
	class ImGuiDecorator {
	public:
		static void init(GLFWwindow*);
		static void preDraw();
		static void draw();
		static void terminate();
	private:
		inline static bool inited = false;
	};
}


