#include "DebugInfo.h"

#include "imgui.h"
#include "componentsModule/ProjectionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"



using namespace GameEngine::Debug;

void DebugInfo::drawInfo() {
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos, 0, {0.f,0.f});
	ImGui::SetNextWindowBgAlpha(0.3f);
	const auto& camera = Engine::getInstance()->getCamera();

	ImGui::Begin("Perf", &opened, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::Text("FPS: %d", Engine::getInstance()->getFPS());
	ImGui::Text("dt: %.4f", static_cast<double>(Engine::getInstance()->getDeltaTime()));
	ImGui::Separator();
	ImGui::Text("camera:");
	ImGui::Text("FOV: %.2f", camera->getComponent<ProjectionComponent>()->getProjection().getFOV());
	ImGui::Text("pos: [%.3f, %.3f, %.3f]", static_cast<double>(camera->getComponent<TransformComponent>()->getPos().x), static_cast<double>(camera->getComponent<TransformComponent>()->getPos().y), static_cast<double>(camera->getComponent<TransformComponent>()->getPos().z));
	auto cameraRotate = camera->getComponent<TransformComponent>()->getRotate();
	ImGui::Text("angle: [%.3f, %.3f, %.3f]", cameraRotate.x, cameraRotate.y, cameraRotate.z);
	ImGui::Text("drawCalls: %zu", RenderModule::Renderer::mDrawCallsCount);
	ImGui::Text("verticesDraw: %zu", RenderModule::Renderer::mDrawVerticesCount);

	ImGui::End();
}
