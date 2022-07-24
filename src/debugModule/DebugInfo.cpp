#include "DebugInfo.h"

#include "imgui.h"
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
	ImGui::Text("pos: [%.3f, %.3f, %.3f]", static_cast<double>(camera->Position.x), static_cast<double>(camera->Position.y), static_cast<double>(camera->Position.z));
	ImGui::Text("drawCalls: %d", RenderModule::Renderer::drawCallsCount);
	ImGui::Text("verticesDraw: %d", RenderModule::Renderer::drawVerticesCount);

	ImGui::End();
}
