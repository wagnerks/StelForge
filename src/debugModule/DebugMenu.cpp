#include "DebugMenu.h"

#include "Benchmark.h"
#include "DebugInfo.h"
#include "imgui.h"
#include "ShadersDebug.h"
#include "componentsModule/CameraComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "glWrapper/Depth.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/SystemManager.h"

using namespace SFE::Debug;

DebugMenu::~DebugMenu() {
	ShadersDebug::terminate();
}

void DebugMenu::draw() {
	FUNCTION_BENCHMARK
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			ImGui::Separator();
			ImGui::DragFloat("camera speed", &ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->movementSpeed, 5.f);
			if (ImGui::DragFloat("screenDrawData.far", &Render::Renderer::screenDrawData.far, 100.f)) {
				GLW::setDepthDistance(Render::Renderer::screenDrawData.far);
				auto camComp = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera());
				camComp->initProjection(camComp->getProjection().getFOV(), camComp->getProjection().getAspect(), camComp->getProjection().getNear(), Render::Renderer::screenDrawData.far);
			}

			if (ImGui::DragFloat("near", &Render::Renderer::screenDrawData.near, 0.1f)) {
				auto camComp = ECSHandler::registry().getComponent<CameraComponent>(ECSHandler::getSystem<SystemsModule::CameraSystem>()->getCurrentCamera());
				camComp->initProjection(camComp->getProjection().getFOV(), camComp->getProjection().getAspect(), Render::Renderer::screenDrawData.near, Render::Renderer::screenDrawData.far);
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
			ImGui::DragInt("max fps", &SFE::Engine::instance()->maxFPS);
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
