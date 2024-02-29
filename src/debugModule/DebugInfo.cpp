#include "DebugInfo.h"

#include "imgui.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "ecss/Registry.h"
#include "renderModule/TextRenderer.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/SystemManager.h"


using namespace SFE::Debug;

void DebugInfo::drawInfo(DebugInfoType type) {
	if (type == DebugInfoType::Small) {
		Render::TextRenderer::instance()->renderText("FPS: " + std::to_string(Engine::instance()->getFPS()), 10.f, 50.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
		Render::TextRenderer::instance()->renderText("dt: " + std::to_string(static_cast<double>(Engine::instance()->getDeltaTime())), 10.f, 80.f, 1.f, Math::Vec3{1.f, 0.f, 0.f}, Render::FontsRegistry::instance()->getFont("fonts/DroidSans.ttf", 20));
	}
	else {
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		auto pos = ImGui::GetMainViewport()->Pos;
		pos.y += 25.f;
		ImGui::SetNextWindowPos(pos, 0, { 0.f,0.f });
		ImGui::SetNextWindowBgAlpha(0.3f);
		const auto& camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();

		ImGui::Begin("Perf", &opened, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
		if (type == DebugInfoType::Middle || type == DebugInfoType::Big) {
			ImGui::Text("FPS: %d", Engine::instance()->getFPS());
			ImGui::Text("dt: %.4f", static_cast<double>(Engine::instance()->getDeltaTime()));
			ImGui::Separator();
			ImGui::Text("camera:");
			ImGui::Text("FOV: %.2f", ECSHandler::registry().getComponent<CameraComponent>(camera)->getProjection().getFOV());
			ImGui::Text("pos: [%.3f, %.3f, %.3f]", static_cast<double>(ECSHandler::registry().getComponent<TransformComponent>(camera)->getPos().x), static_cast<double>(ECSHandler::registry().getComponent<TransformComponent>(camera)->getPos().y), static_cast<double>(ECSHandler::registry().getComponent<TransformComponent>(camera)->getPos().z));
			auto cameraRotate = ECSHandler::registry().getComponent<TransformComponent>(camera)->getRotate();
			ImGui::Text("angle: [%.3f, %.3f, %.3f]", cameraRotate.x, cameraRotate.y, cameraRotate.z);
		}
		
		ImGui::End();
	}	
}
