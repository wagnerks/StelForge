#include "DebugInfo.h"

#include "imgui.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"


using namespace Engine::Debug;

void DebugInfo::drawInfo(DebugInfoType type) {
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	auto pos = ImGui::GetMainViewport()->Pos;
	pos.y += 20.f;
	ImGui::SetNextWindowPos(pos, 0, { 0.f,0.f });
	ImGui::SetNextWindowBgAlpha(0.3f);
	const auto& camera = ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera();
	
	ImGui::Begin("Perf", &opened, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
	if (type == DebugInfoType::Small || type == DebugInfoType::Middle || type == DebugInfoType::Big) {
		ImGui::Text("FPS: %d", UnnamedEngine::instance()->getFPS());
		ImGui::Text("dt: %.4f", static_cast<double>(UnnamedEngine::instance()->getDeltaTime()));
	}

	if (type == DebugInfoType::Middle || type == DebugInfoType::Big) {
		ImGui::Separator();
		ImGui::Text("camera:");
		ImGui::Text("FOV: %.2f", camera->getComponent<CameraComponent>()->getProjection().getFOV());
		ImGui::Text("pos: [%.3f, %.3f, %.3f]", static_cast<double>(camera->getComponent<TransformComponent>()->getPos().x), static_cast<double>(camera->getComponent<TransformComponent>()->getPos().y), static_cast<double>(camera->getComponent<TransformComponent>()->getPos().z));
		auto cameraRotate = camera->getComponent<TransformComponent>()->getRotate();
		ImGui::Text("angle: [%.3f, %.3f, %.3f]", cameraRotate.x, cameraRotate.y, cameraRotate.z);
	}

	if (type == DebugInfoType::Big) {
		ImGui::Text("drawCalls: %zu", RenderModule::Renderer::mDrawCallsCount);
		ImGui::Text("verticesDraw: %zu", RenderModule::Renderer::mDrawVerticesCount);
	}

	ImGui::End();
}
