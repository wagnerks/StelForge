#include "ShadersDebug.h"

#include <string>

#include "imgui.h"

#include "shaderModule/ShaderController.h"
#include "shaderModule/GeometryShader.h"

using namespace GameEngine::Debug;

void ShadersDebug::shadersDebugDraw(bool& opened) {
	if (!opened) {
		return;
	}

	ImGui::Begin("shaders", &opened);
	for (auto& [hash, shader] : SHADER_CONTROLLER->getShaders()) {

		auto shaderIdString = std::to_string(shader->getID());
		if (ImGui::TreeNode(shaderIdString.c_str())) {
			if (auto vfShader = dynamic_cast<ShaderModule::Shader*>(shader)) {
				ImGui::Text("VERTEX FRAGMENT SHADER");

				ImGui::Text("vertexPath: %s", vfShader->getVertexPath().data());
				ImGui::Text("fragmentPath: %s", vfShader->getFragmentPath().data());
			}
			else if (auto gShader = dynamic_cast<ShaderModule::GeometryShader*>(shader)) {
				ImGui::Text("GEOMETRY SHADER");

				ImGui::Text("vertexPath: %s", gShader->getVertexPath().data());
				ImGui::Text("fragmentPath: %s", gShader->getFragmentPath().data());
				ImGui::Text("geometryPath: %s", gShader->getGeometryPath().data());
			}
			
			auto btnId = "reCompile##" + std::to_string(shader->getID());
			if(ImGui::Button(btnId.c_str())) {
				SHADER_CONTROLLER->recompileShader(shader);
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();
}
