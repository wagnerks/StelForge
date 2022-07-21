#include "ShadersDebug.h"

#include <string>

#include "imgui.h"
#include "core/shader.h"
#include "core/ShaderController.h"
#include "misc/cpp/imgui_stdlib.h"

using namespace GameEngine::Debug;

void ShadersDebug::shadersDebugDraw(bool& opened) {
	if (!opened) {
		return;
	}

	ImGui::Begin("shaders", &opened);
	auto paths = SHADER_CONTROLLER->getShaderPaths();
	for (auto& [hash, shader] : SHADER_CONTROLLER->getShaders()) {

		auto shaderIdString = std::to_string(shader->getID());
		if (ImGui::TreeNode(shaderIdString.c_str())) {
			ImGui::Text("vertexPath: %s", paths[hash].first.c_str());
			ImGui::Text("vertexPath: %s", paths[hash].second.c_str());
			auto btnId = "reCompile##" + std::to_string(shader->getID());
			if(ImGui::Button(btnId.c_str())) {
				SHADER_CONTROLLER->recompileShader(shader);
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();
}
