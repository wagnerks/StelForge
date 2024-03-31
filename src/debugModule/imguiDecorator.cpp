#include "imguiDecorator.h"

#include <GLFW/glfw3.h>

#include "Benchmark.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace SFE::Debug {
	void ImGuiDecorator::init(GLFWwindow* window) {
		if (context || !window) {
			return;
		}

		IMGUI_CHECKVERSION();
		context = ImGui::CreateContext();

		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable;

		setStyle();

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 330");
	}

	void ImGuiDecorator::setStyle() {
		ImGuiIO& io = ImGui::GetIO();

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();

		style.WindowRounding = 0.0f;

		//style.Colors[ImGuiCol_Text] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.8f);						// Background of normal windows
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.8f);							// Background of child windows
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);							// Background of popupsf, menusf, tooltips windows
		//style.Colors[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);							// Background of checkboxf, radio buttonf, plotf, sliderf, text input
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);

		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
		//style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.24f, 0.24f, 0.24f, 0.1f);
		//style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_CheckMark] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);

		style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);

		style.Colors[ImGuiCol_Header] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);                // Header* colors are used for CollapsingHeaderf, TreeNodef, Selectablef, MenuItem
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

		//style.Colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);            // Resize grip in lower-right and lower-left corners of windows.
		//style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);

		style.Colors[ImGuiCol_Tab] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);                     // TabItem in a TabBar
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);

		//style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);          // Preview overlay color when about to docking something
		//style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);          // Background color for empty node (e.g. CentralNode with no window docked into it)
		//style.Colors[ImGuiCol_PlotLines] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);         // Table header background
		//style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);     // Table outer and header borders (prefer using Alpha=1.0 here)
		//style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);      // Table inner borders (prefer using Alpha=1.0 here)
		//style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);            // Table row background (even rows)
		//style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);         // Table row background (odd rows)
		//style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
		//style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);        // Rectangle highlighting a drop target
		//style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);          // Gamepad/keyboard: current highlighted item
		//style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f); // Highlight window when using CTRL+TAB
		//style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);     // Darken/colorize entire screen behind the CTRL+TAB window listf, when active
		//style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);      // Darken/colorize entire screen behind a modal windowf, when one is active


#ifdef __APPLE__
		io.FontGlobalScale = 1.f;
#else
		io.FontGlobalScale = 1.5f;
#endif
	}

	void ImGuiDecorator::preDraw() {
		if (!context) {
			return;
		}

		FUNCTION_BENCHMARK;
		previous = ImGui::GetCurrentContext();
		if (previous == context) {
			previous = nullptr;
		}
		else {
			ImGui::SetCurrentContext(context);
		}


		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiDecorator::draw() {
		if (!context) {
			return;
		}
		FUNCTION_BENCHMARK;

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			auto backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		if (previous) {
			ImGui::SetCurrentContext(previous);
			previous = nullptr;
		}
	}

	void ImGuiDecorator::destroyContext() {
		if (!context) {
			return;
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		context = nullptr;
	}
}