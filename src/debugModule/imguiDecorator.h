#pragma once

struct ImGuiContext;
struct GLFWwindow;

namespace SFE::Debug {
	class ImGuiDecorator {
	public:
		void init(GLFWwindow*);
		void setStyle();
		void preDraw();
		void draw();
		void destroyContext();

	public:
		ImGuiContext* context = nullptr;
		ImGuiContext* previous = nullptr;
	};
}


