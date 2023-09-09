#include "ComponentsDebug.h"

#include <algorithm>

#include "imgui.h"
#include "componentsModule/FrustumComponent.h"
#include "componentsModule/LightComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/RenderComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "ecsModule/ECSHandler.h"
#include "ecsModule/EntityBase.h"
#include "ecsModule/EntityManager.h"

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
		auto entityManager = ecsModule::ECSHandler::entityManagerInstance();


		ImGui::Columns(2);
		ImGui::BeginChild("##entities");
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
			ImGui::NextColumn();

			ImGui::BeginChild("##comps");
			ImGui::Text("entity: %s", currentEntity->getNodeId().data());
			ImGui::Text("entity id: %zu", currentEntity->getEntityID());
			ImGui::Separator();



			if (auto comp = currentEntity->getComponent<TransformComponent>(); comp && ImGui::TreeNode("Transform Component")) {
				componentEditorInternal(comp);
				ImGui::TreePop();
			}

			/*	if (auto comp = currentEntity->getComponent<LodComponent>(); comp && ImGui::TreeNode("LOD Component")) {
					componentEditorInternal(comp);
					ImGui::TreePop();
				}*/

				/*if (auto comp = currentEntity->getComponent<MeshComponent>(); comp && ImGui::TreeNode("Mesh Component")) {
					componentEditorInternal(comp);
					ImGui::TreePop();
				}*/

			if (auto comp = currentEntity->getComponent<MaterialComponent>(); comp && ImGui::TreeNode("Material Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<LightComponent>(); comp && ImGui::TreeNode("Light Component")) {
				componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<CameraComponent>(); comp && ImGui::TreeNode("Camera Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<RenderComponent>(); comp && ImGui::TreeNode("Render Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			if (auto comp = currentEntity->getComponent<FrustumComponent>(); comp && ImGui::TreeNode("Frustum Component")) {
				//componentEditorInternal(comp);
				ImGui::TreePop();
			}

			ImGui::EndChild();
		}
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

	if (ImGui::DragFloat3("Pos", posV, 0.001f)) {
		component->setPos({ posV[0], posV[1], posV[2] });
	}

	float rotations[] = { component->getRotate().x, component->getRotate().y, component->getRotate().z };
	if (ImGui::DragFloat3("Rotate", rotations)) {
		component->setRotate({ rotations[0], rotations[1], rotations[2] });
	}

	if (ImGui::DragFloat3("Scale", scaleV, 0.1f)) {
		component->setScale({ scaleV[0], scaleV[1], scaleV[2] });
	}

	auto t = component->getTransform();

	float line1[4] = { t[0][0], t[0][1], t[0][2], t[0][3] };
	float line2[4] = { t[1][0], t[1][1], t[1][2], t[1][3] };
	float line3[4] = { t[2][0], t[2][1], t[2][2], t[2][3] };
	float line4[4] = { t[3][0], t[3][1], t[3][2], t[3][3] };

	if (ImGui::DragFloat4("1", line1)) {
		t[0][0] = line1[0];
		t[0][1] = line1[1];
		t[0][2] = line1[2];
		t[0][3] = line1[3];
	}
	if (ImGui::DragFloat4("2", line2)) {
		t[1][0] = line2[0];
		t[1][1] = line2[1];
		t[1][2] = line2[2];
		t[1][3] = line2[3];
	}
	if (ImGui::DragFloat4("3", line3)) {
		t[2][0] = line3[0];
		t[2][1] = line3[1];
		t[2][2] = line3[2];
		t[2][3] = line3[3];
	}
	if (ImGui::DragFloat4("4", line4)) {
		t[3][0] = line4[0];
		t[3][1] = line4[1];
		t[3][2] = line4[2];
		t[3][3] = line4[3];
	}

	component->setTransform(t);

	auto globalScale = component->getScale(true);
	glm::mat4 rotation = {};
	rotation[0][0] = t[0][0] / globalScale.x;
	rotation[0][1] = t[0][1] / globalScale.x;
	rotation[0][2] = t[0][2] / globalScale.x;

	rotation[1][0] = t[1][0] / globalScale.y;
	rotation[1][1] = t[1][1] / globalScale.y;
	rotation[1][2] = t[1][2] / globalScale.y;

	rotation[2][0] = t[2][0] / globalScale.z;
	rotation[2][1] = t[2][1] / globalScale.z;
	rotation[2][2] = t[2][2] / globalScale.z;
	auto& r = rotation;
	float rline1[4] = { r[0][0], r[0][1], r[0][2], r[0][3] };
	float rline2[4] = { r[1][0], r[1][1], r[1][2], r[1][3] };
	float rline3[4] = { r[2][0], r[2][1], r[2][2], r[2][3] };
	float rline4[4] = { r[3][0], r[3][1], r[3][2], r[3][3] };

	if (ImGui::DragFloat4("r1", rline1)) {
	}
	if (ImGui::DragFloat4("r2", rline2)) {
	}
	if (ImGui::DragFloat4("r3", rline3)) {
	}
	if (ImGui::DragFloat4("r4", rline4)) {
	}

	float rX = -glm::degrees(glm::atan(r[2][1], r[2][2]));
	float rY = -glm::degrees(glm::atan(-r[2][0], glm::sqrt(r[2][1] * r[2][1] + r[2][2] * r[2][2])));
	float rZ = -glm::degrees(glm::atan(r[1][0], r[0][0]));

	auto& view = component->getViewMatrix();
	float viewline1[4] = { view[0][0], view[0][1], view[0][2], view[0][3] };
	float viewline2[4] = { view[1][0], view[1][1], view[1][2], view[1][3] };
	float viewline3[4] = { view[2][0], view[2][1], view[2][2], view[2][3] };
	float viewline4[4] = { view[3][0], view[3][1], view[3][2], r[3][3] };

	if (ImGui::DragFloat4("view1", viewline1)) {
	}
	if (ImGui::DragFloat4("view1", viewline2)) {
	}
	if (ImGui::DragFloat4("view1", viewline3)) {
	}
	if (ImGui::DragFloat4("view1", viewline4)) {
	}

	float globalRot[] = { rX, rY, rZ };
	if (ImGui::DragFloat3("RotateGlobal", globalRot)) {
		component->setRotate({ globalRot[0], globalRot[1], globalRot[2] });
	}

}
//
//void ComponentsDebug::componentEditorInternal(LodComponent* component) {
//	if (!component) {
//		return;
//	}
//
//	auto& lodLevels = const_cast<std::vector<float>&>(component->getLodLevelValues());
//	int i = 0;
//
//	bool needSort = false;
//	for (auto& level : lodLevels) {
//		std::string levelId = "level" + std::to_string(i);
//		if (ImGui::DragFloat(levelId.c_str(), &level, 0.001f)) {
//			needSort = true;
//		}
//		i++;
//	}
//
//	if (needSort) {
//		std::ranges::sort(lodLevels);
//		std::ranges::reverse(lodLevels);
//	}
//
//	if (component->getLodType() == ComponentsModule::eLodType::DISTANCE) {
//		ImGui::Text("Type: %s", "DISTANCE");
//	}
//	else if (component->getLodType() == ComponentsModule::eLodType::SCREEN_SPACE) {
//		ImGui::Text("Type: %s", "SCREEN_SPACE");
//	}
//
//	ImGui::Text("%zu", component->getLodLevel());
//	ImGui::Text("%f", component->getCurrentLodValue());
//}

void ComponentsDebug::componentEditorInternal(ComponentsModule::LightComponent* component) {
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
}

//void ComponentsDebug::componentEditorInternal(ComponentsModule::MeshComponent* component) {
//	if (!component) {
//		return;
//	}
//
//	auto lods = component->getMeshes();
//	ImGui::Text("LODS: %d", lods.size());
//	int i = 0;
//	for (auto& lod : lods) {
//		auto treeLabel = "LOD " + std::to_string(i) + "##meshLod";
//		if (ImGui::TreeNode(std::to_string(i).c_str())) {
//
//			ImGui::Text("vertices: %d", lod.mData.mVertices.size());
//			ImGui::Text("indices: %d", lod.mData.mIndices.size());
//			ImGui::Spacing();
//
//			ImGui::Text("diffuse:");
//			if (lod.mMaterial.mDiffuse.mTexture.isValid()) {
//				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mDiffuse.mTexture.mId), { 200.f,200.f });
//			}
//			else {
//				ImGui::SameLine();
//				ImGui::Text("none");
//			}
//
//			ImGui::Text("specular:");
//			if (lod.mMaterial.mSpecular.mTexture.isValid()) {
//				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mSpecular.mTexture.mId), { 200.f,200.f });
//			}
//			else {
//				ImGui::SameLine();
//				ImGui::Text("none");
//			}
//
//			ImGui::Text("normal:");
//			if (lod.mMaterial.mNormal.mTexture.isValid()) {
//				ImGui::Image(reinterpret_cast<ImTextureID>(lod.mMaterial.mNormal.mTexture.mId), { 200.f,200.f });
//			}
//			else {
//				ImGui::SameLine();
//				ImGui::Text("none");
//			}
//
//
//			ImGui::TreePop();
//		}
//		i++;
//
//
//	}
//
//}
