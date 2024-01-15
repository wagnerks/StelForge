#include "ShadersDebug.h"

#include <string>

#include "imgui.h"
#include "assetsModule/TextureHandler.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "assetsModule/shaderModule/GeometryShader.h"
#include "core/FileSystem.h"
#include "misc/cpp/imgui_stdlib.h"

using namespace Engine::Debug;

void ShadersDebug::shadersDebugDraw(bool& opened) {
	if (opened) {
		ImGui::Begin("shaders", &opened);
		for (auto& [hash, shader] : SHADER_CONTROLLER->getShaders()) {
			ImGui::PushStyleColor(ImGuiCol_Button, { 0,0,0,0 });
			auto btnId = "recompile##" + std::to_string(shader->getID());
			auto updateTex = AssetsModule::TextureHandler::instance()->loadTexture("icons/update-12-32.png");
			auto editTex = AssetsModule::TextureHandler::instance()->loadTexture("icons/edit-47-32.png");

			auto size = ImGui::GetTextLineHeight() - 4;
			static ImVec4 icoColor = { 1,1,1,1 };

			auto drawUpdateBtn = [size, updateTex, shader, btnId]() {

				ImGui::SameLine(0, 20);
				ImGui::PushID(btnId.c_str());
				if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(updateTex->mId), { size,size }, { 0,0 }, { 1,1 }, -1, { 0,0,0,0 }, icoColor)) {
					SHADER_CONTROLLER->recompileShader(shader);
				}
				ImGui::PopID();
			};

			auto drawEditBtn = [this, size, editTex, shader]() {
				auto btnId = "edit##" + std::to_string(shader->getID());
				ImGui::SameLine();
				ImGui::PushID(btnId.c_str());
				if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(editTex->mId), { size,size }, { 0,0 }, { 1,1 }, -1, { 0,0,0,0 }, icoColor)) {
					if (auto it = std::find(mOpenedShaderDebugWindows.begin(), mOpenedShaderDebugWindows.end(), shader->getID()); it == mOpenedShaderDebugWindows.end()) {
						mOpenedShaderDebugWindows.push_back(shader->getID());
					}
				}
				ImGui::PopID();
			};

			if (auto vfShader = dynamic_cast<ShaderModule::Shader*>(shader)) {
				if (ImGui::TreeNode((std::to_string(shader->getID()) + " : " + std::string(vfShader->getFragmentPath())).c_str())) {
					drawUpdateBtn();
					drawEditBtn();

					ImGui::Text("VERTEX FRAGMENT SHADER:  ID: %s", std::to_string(shader->getID()).c_str());

					ImGui::Text("vertexPath: %s", vfShader->getVertexPath().data());
					ImGui::Text("fragmentPath: %s", vfShader->getFragmentPath().data());


					ImGui::Separator();
					ImGui::TreePop();
				}
				else {
					drawUpdateBtn();
					drawEditBtn();
				}
			}
			else if (auto gShader = dynamic_cast<ShaderModule::GeometryShader*>(shader)) {


				if (ImGui::TreeNode((std::to_string(shader->getID()) + " : " + std::string(gShader->getFragmentPath())).c_str())) {
					drawUpdateBtn();
					drawEditBtn();
					ImGui::Text("GEOMETRY SHADER:  ID: %s", std::to_string(shader->getID()).c_str());

					ImGui::Text("vertexPath: %s", gShader->getVertexPath().data());
					ImGui::Text("fragmentPath: %s", gShader->getFragmentPath().data());
					ImGui::Text("geometryPath: %s", gShader->getGeometryPath().data());


					ImGui::Separator();
					ImGui::TreePop();
				}
				else {
					drawUpdateBtn();
					drawEditBtn();
				}
			}
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}

	for (auto it = mOpenedShaderDebugWindows.begin(); it != mOpenedShaderDebugWindows.end(); ) {
		auto shader = SHADER_CONTROLLER->getShader(*it);
		if (!shader) {
			continue;
		}
		static bool clickedSeparator = false;
		static float separatorPos = 0.4f;
		static ImVec2 mousePos = {};

		bool editWindowOpened = true;
		auto* fShader = dynamic_cast<ShaderModule::Shader*>(shader);
		auto* gShader = dynamic_cast<ShaderModule::GeometryShader*>(shader);
		auto strId = std::to_string(*it);

		ImGui::Begin(("shader edit " + std::to_string(*it)).c_str(), &editWindowOpened);
		if (fShader) {
			ImGui::BeginChild("##shaders code", { 0.f, ImGui::GetWindowHeight() * 0.75f });
			ImGui::Text("vertex");
			ImGui::InputTextMultiline(("##vertex" + strId).c_str(), &fShader->vertexCode, { ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ScrollbarSize, ImGui::GetWindowHeight() * 0.8f * (1.f - separatorPos) });

			ImGui::Button("--", { ImGui::GetWindowContentRegionWidth(), 5.f });

			if (ImGui::IsItemClicked()) {
				clickedSeparator = true;
				mousePos = ImGui::GetMousePos();
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}

			if (ImGui::GetIO().MouseReleased[0]) {
				clickedSeparator = false;
			}

			if (clickedSeparator) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				auto dragDelta = mousePos.y - ImGui::GetMousePos().y;
				mousePos = ImGui::GetMousePos();

				separatorPos += dragDelta / ImGui::GetWindowHeight();
				if (separatorPos < 0.1f) {
					separatorPos = 0.1f;
				}
				else if (separatorPos > 0.9f) {
					separatorPos = 0.9f;
				}
			}

			ImGui::Text("fragment");
			ImGui::InputTextMultiline(("##fragment" + strId).c_str(), &fShader->fragmentCode, { ImGui::GetWindowContentRegionWidth() - ImGui::GetStyle().ScrollbarSize, ImGui::GetWindowHeight() * 0.8f * separatorPos });
			ImGui::EndChild();

			if (ImGui::Button("compile")) {
				FileSystem::writeFile(fShader->getVertexPath(), fShader->vertexCode);
				FileSystem::writeFile(fShader->getFragmentPath(), fShader->fragmentCode);
				SHADER_CONTROLLER->recompileShader(shader);
				*it = shader->getID();
			}

		}
		else if (gShader) {
			ImGui::Text("geometry shader type");
		}
		else {
			ImGui::Text("unknown shader type");
		}


		ImGui::End();

		if (!editWindowOpened) {
			it = mOpenedShaderDebugWindows.erase(it);
		}
		else {
			++it;
		}
	}
}
