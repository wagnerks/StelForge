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

	for (auto& [path, shader] : SHADER_CONTROLLER->getShaders()) {
		auto shaderIdString = std::to_string(shader->getID()) + path;
		if (ImGui::TreeNode(shaderIdString.c_str())) {
			auto vertexId = shaderIdString + ": Vertex##" + shaderIdString;
			auto fragmentId = shaderIdString + ": Fragment##" + shaderIdString;
			auto btnId = "compile##" + std::to_string(shader->getID());

			ImGui::Begin(vertexId.c_str());
			ImGui::InputTextMultiline(vertexId.c_str(), &shader->vertexCode, ImGui::GetWindowSize());
			if(ImGui::Button(btnId.c_str())) {
				shader->compileShader();
			}
			ImGui::End();

			ImGui::Begin(fragmentId.c_str());
			ImGui::InputTextMultiline(fragmentId.c_str(), &shader->fragmentCode, ImGui::GetWindowSize());
			if(ImGui::Button(btnId.c_str())) {
				shader->compileShader();
			}
			ImGui::End();

			
			

			ImGui::TreePop();
		}
	}
	ImGui::End();
}
