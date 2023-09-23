#include "DebugMenu.h"

#include "DebugInfo.h"
#include "imgui.h"
#include "ShadersDebug.h"
#include "core/ECSHandler.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"

using namespace Engine::Debug;

void DebugMenu::draw() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			ImGui::Separator();
			ImGui::DragFloat("camera speed", &ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->MovementSpeed, 5.f);
			ImGui::Separator();
			ImGui::Checkbox("debug info", &debugInfoOpened);
			if (ImGui::BeginMenu("Debug info type")) {
				if (ImGui::RadioButton("small", debugInfoType == DebugInfoType::Small)) { debugInfoType = DebugInfoType::Small; }
				if (ImGui::RadioButton("middle", debugInfoType == DebugInfoType::Middle)) { debugInfoType = DebugInfoType::Middle; }
				if (ImGui::RadioButton("big", debugInfoType == DebugInfoType::Big)) { debugInfoType = DebugInfoType::Big; }
				ImGui::EndMenu();
			}

			ImGui::Separator();
			ImGui::Checkbox("shaders debug", &shadersDebugOpened);
			ImGui::Checkbox("imgui demo", &imguiDemo);

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (imguiDemo) {
		ImGui::ShowDemoWindow(&imguiDemo);
	}

	if (debugInfoOpened) {
		DebugInfo::drawInfo(debugInfoType);
	}

	ShadersDebug::instance()->shadersDebugDraw(shadersDebugOpened);
}
