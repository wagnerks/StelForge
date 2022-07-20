#include "DebugMenu.h"

#include "DebugInfo.h"
#include "imgui.h"
#include "ShadersDebug.h"

using namespace GameEngine::Debug;

void DebugMenu::draw() {
	ImGui::Begin("DebugMenu", &opened);
	ImGui::Checkbox("debug info", &debugInfoOpened);
	ImGui::Checkbox("shaders debug", &shadersDebugOpened);
	ImGui::End();

	if (debugInfoOpened) {
		DebugInfo::drawInfo();
	}
	
	ShadersDebug::shadersDebugDraw(shadersDebugOpened);
}
