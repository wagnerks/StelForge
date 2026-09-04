#pragma once

struct ImGuiContext;
struct GLFWwindow;

namespace SFE::Debug {
	void applyDefaultImGuiStyle();

	class ImGuiDecorator {
	public:
		void init(GLFWwindow* window, bool viewports = true);
		void setStyle();
		void preDraw();
		void draw();
		void destroyContext();

	public:
		ImGuiContext* context = nullptr;
		ImGuiContext* previous = nullptr;
	};
}
