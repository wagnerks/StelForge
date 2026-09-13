#pragma once

struct ImGuiContext;
struct GLFWwindow;

namespace SFE::Debug {
	void applyDefaultImGuiStyle();
	// Call between the platform backend's NewFrame and ImGui::NewFrame.
	void freezeImGuiForCapture();

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
