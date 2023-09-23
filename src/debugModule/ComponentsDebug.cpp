#include "ComponentsDebug.h"

#include <algorithm>
#include <gtx/quaternion.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "core/FileSystem.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/OutlineComponent.h"
#include "core/ECSHandler.h"
#include "ecsModule/EntityComponentSystem.h"
#include "ecsModule/EntityBase.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/CameraSystem.h"
#include "renderModule/Utils.h"
#include "misc/cpp/imgui_stdlib.h"

using namespace Engine::Debug;

void ComponentsDebug::drawTree(ecsModule::EntityInterface* entity, size_t& selectedID) {
	auto id = entity->getEntityID();
	auto& children = entity->getElements();

	if (!children.empty()) {
		int flag = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
		if (selectedID == id) {
			flag = flag | ImGuiTreeNodeFlags_Selected;
		}

		if (ImGui::TreeNodeEx((std::string(entity->getNodeId()) + ":" + std::to_string(id)).c_str(), flag)) {
			if (ImGui::IsItemClicked()) {
				selectedID = id;
			}

			for (auto child : children) {
				drawTree(child, selectedID);
			}

			ImGui::TreePop();
		}
		else {
			if (ImGui::IsItemClicked()) {
				selectedID = id;
			}
		}
	}
	else {
		if (ImGui::Selectable((std::string(entity->getNodeId()) + ":" + std::to_string(id)).c_str(), selectedID == id)) {
			selectedID = id;
		}
	}
}

void ComponentsDebug::entitiesDebug() {

	if (ImGui::Begin("Entities Editor")) {

		static bool clickedSeparator = false;
		static float separatorPos = 100.f;
		static ImVec2 mousePos = {};


		auto entityManager = ECSHandler::entityManagerInstance();

		if (auto currentEntity = entityManager->getEntity(mSelectedId)) {
			currentEntity->removeComponent<OutlineComponent>();
		}

		if (separatorPos > ImGui::GetWindowHeight() * 0.9f) {
			separatorPos = ImGui::GetWindowHeight() * 0.9f;
		}
		else if (separatorPos < ImGui::GetWindowHeight() * 0.1f) {
			separatorPos = ImGui::GetWindowHeight() * 0.1f;
		}

		ImGui::BeginChild("##entities", { ImGui::GetWindowContentRegionWidth(), separatorPos });
		for (auto entity : entityManager->getAllEntities()) {
			if (!entity) {
				continue;
			}

			if (entity->getParent()) {
				continue;
			}

			drawTree(entity, mSelectedId);
		}
		ImGui::EndChild();

		if (auto currentEntity = entityManager->getEntity(mSelectedId)) {
			currentEntity->addComponent<OutlineComponent>();
		}

		ImGui::Separator();
		ImGui::Button("--", { ImGui::GetWindowContentRegionWidth(), 5.f });

		if (ImGui::IsItemClicked()) {
			clickedSeparator = true;
			mousePos = ImGui::GetMousePos();
		}

		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
		ImGui::Separator();

		if (ImGui::GetIO().MouseReleased[0]) {
			clickedSeparator = false;
		}

		if (clickedSeparator) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			auto dragDelta = mousePos.y - ImGui::GetMousePos().y;
			mousePos = ImGui::GetMousePos();

			separatorPos -= dragDelta;
		}



		ImGui::BeginChild("comps##comps", {}, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

		if (auto currentEntity = entityManager->getEntity(mSelectedId)) {

			ImGui::Text("entity: \"%s\"", currentEntity->getNodeId().data());
			ImGui::SameLine();
			ImGui::Text("id: %zu", currentEntity->getEntityID());
			ImGui::Separator();

			bool isDrawable = currentEntity->getComponent<IsDrawableComponent>();
			if (ImGui::Checkbox("isDrawable", &isDrawable)) {
				if (isDrawable) {
					currentEntity->addComponent<IsDrawableComponent>();
					for (auto node : currentEntity->getAllNodes()) {
						node->addComponent<IsDrawableComponent>();
					}
				}
				else {
					currentEntity->removeComponent<IsDrawableComponent>();
					for (auto node : currentEntity->getAllNodes()) {
						node->removeComponent<IsDrawableComponent>();
					}
				}
			}

			bool outline = currentEntity->getComponent<OutlineComponent>();
			if (ImGui::Checkbox("outline", &outline)) {
				if (outline) {
					currentEntity->addComponent<OutlineComponent>();
				}
				else {
					currentEntity->removeComponent<OutlineComponent>();
				}
			}

			ImGui::Separator();

			auto& P = ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<CameraComponent>()->getProjection().getProjectionsMatrix();
			auto& V = ECSHandler::systemManagerInstance()->getSystem<SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<TransformComponent>()->getViewMatrix();
			auto PV = P * V;
			auto S = glm::scale(glm::mat4(1.0f), glm::vec3(1.f, 1.f, 1.f));

			if (auto comp = currentEntity->getComponent<TransformComponent>(); comp && ImGui::TreeNode("Transform Component")) {
				componentEditorInternal(comp);

				auto pos = glm::vec3{ 0.f,0.f,0.f };

				auto rotQuat = glm::quat({ 0.f, 0.f,0.f });

				auto xyzLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
				xyzLines->use();

				auto model = currentEntity->getComponent<TransformComponent>()->getTransform();


				model *= glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(rotQuat) * S;

				xyzLines->setMat4("PVM", PV * model);
				glm::vec3 start = {};
				glm::vec3 end = { 0.f,0.f, -150.f };

				RenderModule::Utils::renderLine(start, end);


				ImGui::TreePop();
			}

			/*	if (auto comp = currentEntity->getComponent<LodComponent>(); comp && ImGui::TreeNode("LOD Component")) {
					componentEditorInternal(comp);
					ImGui::TreePop();
				}*/

			if (auto comp = currentEntity->getComponent<ModelComponent>(); comp && ImGui::TreeNode("Mesh Component")) {
				auto& modObj = comp->getModel(0);

				static bool drawNormales = false;
				static bool drawTangent = false;
				static bool drawBiTangent = false;
				static bool vertexNormals = false;

				ImGui::Checkbox("drawNormals", &drawNormales);
				ImGui::Checkbox("drawTangent", &drawTangent);
				ImGui::Checkbox("drawBiTangent", &drawBiTangent);

				if (ImGui::RadioButton("vertex normales", vertexNormals)) {
					vertexNormals = true;
				}
				if (ImGui::RadioButton("face normales", !vertexNormals)) {
					vertexNormals = false;
				}

				if (drawNormales || drawTangent || drawBiTangent) {
					auto xyzLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
					xyzLines->use();

					auto model = currentEntity->getComponent<TransformComponent>()->getTransform();

					xyzLines->setMat4("PVM", PV * model);

					//vertex normals
					if (vertexNormals) {
						for (auto& mesh : modObj.mMeshHandles) {
							for (int i = 0; i < mesh.mData.mVertices.size(); i++) {
								auto pos = mesh.mData.mVertices[i].mPosition;

								if (drawNormales) {
									auto posEnd = pos + mesh.mData.mVertices[i].mNormal * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}
								if (drawTangent) {
									auto posEnd = pos + mesh.mData.mVertices[i].mTangent * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}
								if (drawBiTangent) {
									auto posEnd = pos + mesh.mData.mVertices[i].mBiTangent * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}

							}
						}
					}
					else {
						//face normals
						for (auto& mesh : modObj.mMeshHandles) {
							for (int i = 0; i < mesh.mData.mIndices.size(); i += 3) {

								auto pos = mesh.mData.mVertices[mesh.mData.mIndices[i]].mPosition;
								pos += mesh.mData.mVertices[mesh.mData.mIndices[i + 1]].mPosition;
								pos += mesh.mData.mVertices[mesh.mData.mIndices[i + 2]].mPosition;
								pos /= 3.f;

								if (drawNormales) {
									auto posEnd = pos + mesh.mData.mVertices[mesh.mData.mIndices[i]].mNormal * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}
								if (drawTangent) {
									auto posEnd = pos + mesh.mData.mVertices[mesh.mData.mIndices[i]].mTangent * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}
								if (drawBiTangent) {
									auto posEnd = pos + mesh.mData.mVertices[mesh.mData.mIndices[i]].mBiTangent * 5.f;
									RenderModule::Utils::renderLine(pos, posEnd);
								}

							}
						}
					}
				}

				componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<MaterialComponent>(); comp && ImGui::TreeNode("Material Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<LightSourceComponent>(); comp && ImGui::TreeNode("Light Component")) {
				componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<CameraComponent>(); comp && ImGui::TreeNode("Camera Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<ComponentsModule::CascadeShadowComponent>(); comp && ImGui::TreeNode("CascadeShadowComponent")) {
				componentEditorInternal(comp);
				ImGui::TreePop();
			}


			if (auto comp = currentEntity->getComponent<FrustumComponent>(); comp && ImGui::TreeNode("Frustum Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}
			if (auto comp = currentEntity->getComponent<ComponentsModule::ShaderComponent>(); comp && ImGui::TreeNode("Shader Component")) {
				componentEditorInternal(comp);
				ImGui::TreePop();
			}



			auto xyzLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
			xyzLines->use();

			auto& model = currentEntity->getComponent<TransformComponent>()->getTransform();

			xyzLines->setMat4("PVM", PV * model);

			RenderModule::Utils::renderXYZ(50.f);

			auto coloredLines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/colored_lines.fs");
			coloredLines->use();

			xyzLines->setMat4("PVM", PV * model);

			RenderModule::Utils::renderCamera();
		}
		ImGui::EndChild();
	}


	ImGui::End();


}

void ComponentsDebug::componentEditorInternal(TransformComponent* component) {
	if (!component) {
		return;
	}

	auto pos = component->getPos();
	auto scale = component->getScale();

	float posV[] = { pos.x, pos.y, pos.z };
	float scaleV[] = { scale.x, scale.y, scale.z };

	if (ImGui::DragFloat3("Pos", posV, 0.1f)) {
		component->setPos({ posV[0], posV[1], posV[2] });
	}

	float rotations[] = { component->getRotate().x, component->getRotate().y, component->getRotate().z };
	if (ImGui::DragFloat3("Rotate", rotations, 0.1f)) {
		component->setRotate({ rotations[0], rotations[1], rotations[2] });
	}

	if (ImGui::DragFloat3("Scale", scaleV, 0.1f)) {
		component->setScale({ scaleV[0], scaleV[1], scaleV[2] });
	}
}

void ComponentsDebug::componentEditorInternal(ComponentsModule::LightSourceComponent* component) {
	if (!component) {
		return;
	}
	ImGui::Text("Light type: ");
	ImGui::SameLine();

	switch (component->getType()) {
	case ComponentsModule::eLightType::DIRECTIONAL: ImGui::Text("DIRECTIONAL"); break;
	case ComponentsModule::eLightType::POINT: ImGui::Text("POINT"); break;
	case ComponentsModule::eLightType::PERSPECTIVE: ImGui::Text("PERSPECTIVE"); break;
	case ComponentsModule::eLightType::NONE: ImGui::Text("NONE"); break;
	}

	ImGui::Separator();

	auto intensity = component->getIntensity();
	if (ImGui::DragFloat("intensity", &intensity, 0.01f)) {
		component->setIntensity(intensity);
	}

	auto biasValue = component->getBias();
	if (ImGui::DragFloat("bias", &biasValue, 0.01f)) {
		component->setBias(biasValue);
	}

	float texelSize[2] = { component->getTexelSize().x, component->getTexelSize().y };
	if (ImGui::DragFloat2("texel size", texelSize, 0.01f)) {
		component->setTexelSize({ texelSize[0], texelSize[1] });
	}

	auto samplesCount = component->getSamples();
	if (ImGui::DragInt("samples", &samplesCount, 1, 1)) {
		if (samplesCount < 1) {
			samplesCount = 1;
		}
		component->setSamples(samplesCount);
	}

	float lightColor[3] = { component->getLightColor().r, component->getLightColor().g, component->getLightColor().b };
	if (ImGui::ColorPicker3("light color", lightColor)) {
		component->setLightColor({ lightColor[0], lightColor[1], lightColor[2] });
	}

	ImGui::DragFloat("linear", &component->mLinear, 0.1f);
	ImGui::DragFloat("quadratic", &component->mQuadratic, 0.01f);
	ImGui::DragFloat("radius", &component->mRadius, 0.1f);
}

void ComponentsDebug::componentEditorInternal(CascadeShadowComponent* component) {
	if (!component) {
		return;
	}

	auto nameCreator = [](std::string name, int idx) {
		return std::string(name + "##" + std::to_string(idx));
	};

	float resolution[2] = { component->resolution.x, component->resolution.y };

	if (ImGui::DragFloat2("resolution", resolution)) {
		component->resolution.x = resolution[0];
		component->resolution.y = resolution[1];
	}

	ImGui::DragFloat("shadows_intensity", &component->shadowIntensity, 0.02f, 0.f, 1.f);
	if (ImGui::TreeNode("cascades")) {
		int i = 0;
		for (auto& cascade : component->cascades) {
			if (ImGui::TreeNode(nameCreator("cascade", i).c_str())) {
				if (ImGui::DragFloat(nameCreator("near", i).c_str(), &component->shadowCascadeLevels[i])) {
					component->markDirty();
				}

				if (ImGui::DragFloat(nameCreator("far", i).c_str(), &component->shadowCascadeLevels[i + 1])) {
					component->markDirty();
				}

				ImGui::Separator();

				ImGui::DragFloat(nameCreator("bias", i).c_str(), &cascade.bias);
				ImGui::DragInt(nameCreator("samples", i).c_str(), &cascade.samples);

				float texel[2] = { cascade.texelSize.x, cascade.texelSize.y };

				if (ImGui::DragFloat2(nameCreator("texel size", i).c_str(), texel)) {
					cascade.texelSize.x = texel[0];
					cascade.texelSize.y = texel[1];
				}

				ImGui::DragFloat(nameCreator("z mult near", i).c_str(), &cascade.zMult.x);
				ImGui::DragFloat(nameCreator("z mult far", i).c_str(), &cascade.zMult.y);

				ImGui::TreePop();
			}
			i++;
		}
		ImGui::TreePop();
	}


	if (ImGui::Button("save to json")) {
		FileSystem::writeJson("cascadedShadows.json", PropertiesModule::PropertiesSystem::serializeEntity(ECSHandler::entityManagerInstance()->getEntity(component->getOwnerId())));
	}

}

void ComponentsDebug::componentEditorInternal(ComponentsModule::ModelComponent* component) {
	if (!component) {
		return;
	}

	if (ImGui::Button("recalculate normals")) {
		auto model = AssetsModule::ModelLoader::instance()->load(component->mPath);
		model->normalizeModel();
		component->init(model);
	}

	auto& modelobj = component->getModel(0);

	int i = 0;
	for (auto& lod : modelobj.mMeshHandles) {
		auto treeLabel = "LOD " + std::to_string(i) + "##meshLod";
		if (ImGui::TreeNode(std::to_string(i).c_str())) {

			ImGui::Text("vertices: %d", lod.mData.mVertices.size());
			ImGui::Text("indices: %d", lod.mData.mIndices.size());
			ImGui::Spacing();

			static std::string diffusePath = "";
			ImGui::InputText("diffusePath", &diffusePath);
			if (ImGui::Button("load##diffuse")) {
				lod.mMaterial.mDiffuse.mTexture = AssetsModule::TextureHandler::instance()->mLoader.loadTexture(diffusePath);
			}

			static std::string normalPath = "";
			ImGui::InputText("normalPath", &normalPath);
			if (ImGui::Button("load##normalPath")) {
				lod.mMaterial.mNormal.mTexture = AssetsModule::TextureHandler::instance()->mLoader.loadTexture(normalPath);
			}

			ImGui::Text("diffuse:");
			if (lod.mMaterial.mDiffuse.mTexture.isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mDiffuse.mTexture.mId), { 200.f,200.f });

			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}

			ImGui::Text("specular:");
			if (lod.mMaterial.mSpecular.mTexture.isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mSpecular.mTexture.mId), { 200.f,200.f });
			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}

			ImGui::Text("normal:");
			if (lod.mMaterial.mNormal.mTexture.isValid()) {
				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mNormal.mTexture.mId), { 200.f,200.f });
			}
			else {
				ImGui::SameLine();
				ImGui::Text("none");
			}


			ImGui::TreePop();
		}
		i++;
	}
}


struct ShaderVariable {
	enum class ShaderVariableType {
		INT, FLOAT, VEC2, VEC3, VEC4, MAT3, MAT4, SAMPLER_2D, UNKNOWN
	};

	inline static ShaderVariableType stringToShaderVariableType(std::string_view stringType) {
		if (stringType == "int") {
			return ShaderVariableType::INT;
		}
		else if (stringType == "float") {
			return ShaderVariableType::FLOAT;
		}
		else if (stringType == "vec2") {
			return ShaderVariableType::VEC2;
		}
		else if (stringType == "vec3") {
			return ShaderVariableType::VEC3;
		}
		else if (stringType == "vec4") {
			return ShaderVariableType::VEC4;
		}
		else if (stringType == "mat3") {
			return ShaderVariableType::MAT3;
		}
		else if (stringType == "mat4") {
			return ShaderVariableType::MAT4;
		}
		else if (stringType == "sampler2D") {
			return ShaderVariableType::SAMPLER_2D;
		}

		return ShaderVariableType::UNKNOWN;
	}

	ShaderVariable(std::string_view type, std::string_view name, size_t arraySize) : type(stringToShaderVariableType(type)), name(name), typeString(type), arraySize(arraySize) {}

	ShaderVariableType type;
	std::string name;
	std::string typeString;
	size_t arraySize = 0;
};

struct ShaderStruct {
	std::string name;
	std::vector<ShaderVariable> variables;
};

void ComponentsDebug::componentEditorInternal(ComponentsModule::ShaderComponent* component) {
	if (!component) {
		return;
	}

	ImGui::Text("shader path, vertex / fragment");

	ImGui::InputText("v##vertexShaderPath", &component->shaderPathVertex);
	ImGui::InputText("f##fragmentShaderPath", &component->shaderPathFragment);

	if (ImGui::Button("load")) {
		auto shader = SHADER_CONTROLLER->loadVertexFragmentShader(component->shaderPathVertex, component->shaderPathFragment);
		SHADER_CONTROLLER->deleteShader(shader);
		shader = SHADER_CONTROLLER->loadVertexFragmentShader(component->shaderPathVertex, component->shaderPathFragment);
		component->shaderId = shader->getID();
	}

	auto shader = dynamic_cast<ShaderModule::Shader*>(SHADER_CONTROLLER->getShader(component->shaderId));
	if (shader) {
		if (ImGui::Button("fill shader uniforms")) {
			std::vector<ShaderVariable> uniforms;
			std::vector<ShaderStruct> structs;
			auto findUniforms = [&uniforms, &structs](std::string code) {
				std::string word = "";

				std::vector<std::string> words;
				bool isStruct = false;
				bool closedStruct = false;

				bool comment = false;

				int longComment = 0;
				char prevSymbol = ' ';
				std::map<std::string, std::string> variables;
				for (const auto& codeSymbol : code) {
					if (codeSymbol == '\n') {
						comment = false;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '/' && codeSymbol == '*') {
						longComment++;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '*' && codeSymbol == '/') {
						longComment--;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (prevSymbol == '/' && codeSymbol == '/') {
						comment = true;
						prevSymbol = codeSymbol;
						continue;
					}
					else if (comment || longComment) { //ignore comments
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == '=') {
						if (word != "") {
							words.push_back(word);
						}

						words.push_back("=");
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == ' ') {
						if (word != "") {
							words.push_back(word);
						}
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == '{') {
						if (words[0] == "struct") {
							structs.push_back({ words[1],{} });

							isStruct = true;
							words.clear();
						}
						word = "";
						prevSymbol = codeSymbol;
						continue;
					}
					else if (codeSymbol == '}') {
						if (isStruct) {
							closedStruct = true;
						}
						prevSymbol = codeSymbol;
						continue;
					}

					if (codeSymbol == ';') {
						if (word != "") {
							words.push_back(word);
						}
						word = "";

						if (!words.empty()) {
							if (isStruct || words[0] == "uniform") {
								if (words[0] == "uniform") {
									words.erase(words.begin());
								}

								auto type = words[0];
								auto name = words[1];

								for (auto i = 2u; i < words.size(); i++) {
									if (words[i] == "=") {
										break;
									}

									words[1] += words[i]; //merge name with '[' 'agea' ']' and ignore spaces
								}

								auto arrayBracket = name.find_first_of('[');
								std::string arrayInfo = "";
								size_t arraySize = 1;

								if (arrayBracket != std::string::npos) {
									arrayInfo = name.substr(arrayBracket + 1, name.size() - arrayBracket - 2);
									name = name.substr(0, arrayBracket);
								}

								if (!arrayInfo.empty()) {
									if (auto& varvalue = variables[arrayInfo]; !varvalue.empty()) {
										arraySize = std::atoi(varvalue.c_str());
									}
									else {
										arraySize = std::atoi(arrayInfo.c_str());
									}
								}

								if (isStruct) {
									structs.back().variables.push_back({ type, name, arraySize });
								}
								else {
									uniforms.push_back({ type, name, arraySize });
								}


								if (closedStruct) {
									isStruct = false;
									closedStruct = false;
								}

							}

							for (size_t i = 0; i < words.size(); i++) {
								if (words[i] == "=") {
									variables[words[i - 1]] = words[i + 1];
								}
							}

							words.clear();
						}
						prevSymbol = codeSymbol;
						continue;
					}

					word += codeSymbol;
					prevSymbol = codeSymbol;
				}
			};


			findUniforms(shader->vertexCode);
			findUniforms(shader->fragmentCode);
			std::function<void(const std::vector<ShaderVariable>& uniforms, ComponentsModule::ShaderVariablesStruct& variables)> fillComponentUniforms;
			fillComponentUniforms = [&fillComponentUniforms, &structs](const std::vector<ShaderVariable>& uniforms, ComponentsModule::ShaderVariablesStruct& variables) {
				for (auto& uniform : uniforms) {
					switch (uniform.type) {
					case ShaderVariable::ShaderVariableType::INT:
						variables.integerUniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::FLOAT:
						variables.floatUniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC2:
						variables.vec2Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC3:
						variables.vec3Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::VEC4:
						variables.vec4Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::MAT3:
						variables.mat3Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::MAT4:
						variables.mat4Uniforms[uniform.name].value.resize(uniform.arraySize);
						break;
					case ShaderVariable::ShaderVariableType::SAMPLER_2D:
						break;
					case ShaderVariable::ShaderVariableType::UNKNOWN:
						for (auto structShader : structs) {
							if (structShader.name == uniform.typeString) {
								variables.structUniforms[uniform.name].value.resize(uniform.arraySize);
								for (auto& str : variables.structUniforms[uniform.name].value) {
									fillComponentUniforms(structShader.variables, str);
								}

								break;
							}
						}
						break;
					default:;
					}
				}
			};

			fillComponentUniforms(uniforms, component->variables);

		}

		std::function<void(ComponentsModule::ShaderVariablesStruct&)> drawShaderVirables = nullptr;
		drawShaderVirables = [&drawShaderVirables](ComponentsModule::ShaderVariablesStruct& variables) {
			for (auto& variable : variables.floatUniforms.data) {
				if (variable.value.size() > 1) {
					for (auto idx = 0u; idx < variable.value.size(); idx++) {
						ImGui::DragFloat((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
					}
				}
				else {
					ImGui::DragFloat((variable.name).c_str(), &variable.value.front());
				}
			}

			for (auto& variable : variables.integerUniforms.data) {
				if (variable.value.size() > 1) {
					for (auto idx = 0u; idx < variable.value.size(); idx++) {
						ImGui::DragInt((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
					}
				}
				else {
					ImGui::DragInt((variable.name).c_str(), &variable.value.front());
				}
			}


			for (auto& str : variables.structUniforms.data) {
				if (ImGui::TreeNode(str.name.c_str())) {
					size_t i = 0;
					for (auto& val : str.value) {
						if (ImGui::TreeNode((str.name + "[" + std::to_string(i) + "]").c_str())) {
							drawShaderVirables(val);
							ImGui::TreePop();
						}
						i++;
					}

					ImGui::TreePop();
				}


			}

			/*for (auto variable : variables.mat4Uniforms.data) {
				if (variable.value.size() > 1) {
					for (auto idx = 0u; idx < variable.value.size(); idx++) {
						ImGui::DragFloat4((variable.name + "[" + std::to_string(idx) + "]").c_str(), &variable.value[idx]);
					}
				}
				else {
					ImGui::DragInt((variable.name).c_str(), &variable.value.front());
				}
			}*/
		};

		drawShaderVirables(component->variables);

		auto& vertex = shader->vertexCode;
		auto& fragment = shader->fragmentCode;

		ImGui::InputTextMultiline("vertex", &vertex);
		ImGui::InputTextMultiline("fragment", &fragment);

		if (ImGui::Button("compile")) {
			FileSystem::writeFile(shader->getVertexPath(), vertex);
			FileSystem::writeFile(shader->getFragmentPath(), fragment);
			SHADER_CONTROLLER->recompileShader(shader);
			component->shaderId = shader->getID();
		}

		if (ImGui::Button("apply")) {
			component->variables.apply(shader);
		}
	}

}