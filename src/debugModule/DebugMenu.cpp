#include "DebugMenu.h"

#include "Benchmark.h"
#include "DebugInfo.h"
#include "imgui.h"
#include "ShadersDebug.h"
#include "componentsModule/CameraComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/SystemManager.h"

using namespace Engine::Debug;

DebugMenu::~DebugMenu() {
	ShadersDebug::terminate();
}

void DebugMenu::draw() {
	FUNCTION_BENCHMARK
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			ImGui::Separator();
			ImGui::DragFloat("camera speed", &ECSHandler::getSystem<Engine::SystemsModule::CameraSystem>()->movementSpeed, 5.f);
			if (ImGui::DragFloat("drawDistance", &RenderModule::Renderer::drawDistance, 100.f)) {
				glClearDepth(RenderModule::Renderer::drawDistance);
				auto camComp = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera().getID());
				camComp->initProjection(camComp->getProjection().getFOV(), camComp->getProjection().getAspect(), camComp->getProjection().getNear(), RenderModule::Renderer::drawDistance);
			}
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
			ImGui::DragInt("max fps", &Engine::UnnamedEngine::instance()->maxFPS);
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
