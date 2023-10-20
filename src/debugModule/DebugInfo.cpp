#include "DebugInfo.h"

#include "imgui.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "ecss/Registry.h"
#include "systemsModule/CameraSystem.h"
#include "systemsModule/SystemManager.h"


using namespace Engine::Debug;

void DebugInfo::drawInfo(DebugInfoType type) {
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	auto pos = ImGui::GetMainViewport()->Pos;
	pos.y += 20.f;
	ImGui::SetNextWindowPos(pos, 0, { 0.f,0.f });
	ImGui::SetNextWindowBgAlpha(0.3f);
	const auto& camera = ECSHandler::systemManager()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();

	ImGui::Begin("Perf", &opened, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
	if (type == DebugInfoType::Small || type == DebugInfoType::Middle || type == DebugInfoType::Big) {
		ImGui::Text("FPS: %d", UnnamedEngine::instance()->getFPS());
		ImGui::Text("dt: %.4f", static_cast<double>(UnnamedEngine::instance()->getDeltaTime()));
	}

	if (type == DebugInfoType::Middle || type == DebugInfoType::Big) {
		ImGui::Separator();
		ImGui::Text("camera:");
		ImGui::Text("FOV: %.2f", ECSHandler::registry()->getComponent<CameraComponent>(camera)->getProjection().getFOV());
		ImGui::Text("pos: [%.3f, %.3f, %.3f]", static_cast<double>(ECSHandler::registry()->getComponent<TransformComponent>(camera)->getPos().x), static_cast<double>(ECSHandler::registry()->getComponent<TransformComponent>(camera)->getPos().y), static_cast<double>(ECSHandler::registry()->getComponent<TransformComponent>(camera)->getPos().z));
		auto cameraRotate = ECSHandler::registry()->getComponent<TransformComponent>(camera)->getRotate();
		ImGui::Text("angle: [%.3f, %.3f, %.3f]", cameraRotate.x, cameraRotate.y, cameraRotate.z);
	}

	if (type == DebugInfoType::Big) {
		ImGui::Text("drawCalls: %zu", RenderModule::Renderer::mDrawCallsCount);
		ImGui::Text("verticesDraw: %zu", RenderModule::Renderer::mDrawVerticesCount);
	}

	ImGui::End();
}
