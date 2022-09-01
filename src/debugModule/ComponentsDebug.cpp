#include "ComponentsDebug.h"

#include <algorithm>

#include "imgui.h"
#include "componentsModule/LodComponent.h"
#include "componentsModule/ModelComponent.h"
#include "core/Engine.h"
#include "ecsModule/EntityManager.h"

using namespace GameEngine::Debug;

void ComponentsDebug::transformComponentDebug(std::string_view id, TransformComponent* transformComp) {
	if (!transformComp) {
		return;
	}

	ImGui::Begin("transforms");
	if (ImGui::TreeNode(id.data())) {
		auto pos = transformComp->getPos();
		auto scale = transformComp->getScale();

		float posV[] = {pos.x, pos.y, pos.z};
		float scaleV[] = {scale.x, scale.y, scale.z};

		if (ImGui::DragFloat3("pos", posV, 0.001f)) {
			transformComp->setPos({posV[0], posV[1], posV[2]});
		}

		float rotations[] = {transformComp->getRotate().x, transformComp->getRotate().y, transformComp->getRotate().z};
		if (ImGui::DragFloat3("rotations", rotations)) {
			transformComp->setRotate({rotations[0], rotations[1], rotations[2]});
		}

		if (ImGui::DragFloat3("scale", scaleV, 0.1f)) {
			transformComp->setScale({scaleV[0], scaleV[1], scaleV[2]});
		}
		transformComp->reloadTransform();
		ImGui::TreePop();
	}
	


	ImGui::End();
}

void ComponentsDebug::entitiesDebug() {
	auto entityManager = ecsModule::ECSHandler::entityManagerInstance();
	static size_t selectedID = 0;
	ecsModule::EntityInterface* currentEntity = nullptr;
	if (ImGui::Begin("Entities Editor")) {
			
		

		ImGui::Columns(2);
		ImGui::BeginChild("##entities");
		for (auto entity : entityManager->getAllEntities()) {
			if (!entity) {
				continue;
			}
			
			
			auto id = entity->getEntityID();

			bool selected = selectedID == id;
			
			if (ImGui::Selectable((std::string(entity->getStringId()) + ":" + std::to_string(id)).c_str(), selected)) {
				selectedID = id;
			}

			if (selected) {
				currentEntity = entity;
			}
		}
		ImGui::EndChild();

		if (currentEntity) {
			ImGui::NextColumn();

			ImGui::BeginChild("##comps");
			ImGui::Text("entity: %s", currentEntity->getStringId().data());
			ImGui::Text("entity id: %d", currentEntity->getEntityID());

			if (ImGui::TreeNode("Transform Component")) {
				auto comp = currentEntity->getComponent<TransformComponent>();
				transformComponentInternal(comp);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("LOD Component")) {
				auto comp = currentEntity->getComponent<LodComponent>();
				lodComponentInternal(comp);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Model Component")) {
				auto comp = currentEntity->getComponent<ModelComponent>();
				modelComponentInternal(comp);
				ImGui::TreePop();
			}
			ImGui::EndChild();
		}
	}


	ImGui::End();
}

void ComponentsDebug::transformComponentInternal(TransformComponent* component) {
	if (!component) {
		return;
	}

	auto pos = component->getPos();
	auto scale = component->getScale();

	float posV[] = {pos.x, pos.y, pos.z};
	float scaleV[] = {scale.x, scale.y, scale.z};

	if (ImGui::DragFloat3("Pos", posV, 0.001f)) {
		component->setPos({posV[0], posV[1], posV[2]});
	}

	float rotations[] = {component->getRotate().x, component->getRotate().y, component->getRotate().z};
	if (ImGui::DragFloat3("Rotate", rotations)) {
		component->setRotate({rotations[0], rotations[1], rotations[2]});
	}

	if (ImGui::DragFloat3("Scale", scaleV, 0.1f)) {
		component->setScale({scaleV[0], scaleV[1], scaleV[2]});
	}

	auto t = component->getTransform();
	
	float line1[4] = {t[0][0], t[0][1], t[0][2], t[0][3]};
	float line2[4] = {t[1][0], t[1][1], t[1][2], t[1][3]};
	float line3[4] = {t[2][0], t[2][1], t[2][2], t[2][3]};
	float line4[4] = {t[3][0], t[3][1], t[3][2], t[3][3]};

	if (ImGui::DragFloat4("1", line1)){
		t[0][0] = line1[0];
		t[0][1] = line1[1];
		t[0][2] = line1[2];
		t[0][3] = line1[3];
	}
	if (ImGui::DragFloat4("2", line2)){
		t[1][0] = line2[0];
		t[1][1] = line2[1];
		t[1][2] = line2[2];
		t[1][3] = line2[3];
	}
	if (ImGui::DragFloat4("3", line3)){
		t[2][0] = line3[0];
		t[2][1] = line3[1];
		t[2][2] = line3[2];
		t[2][3] = line3[3];
	}
	if (ImGui::DragFloat4("4", line4)){
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
	float rline1[4] = {r[0][0], r[0][1], r[0][2], r[0][3]};
	float rline2[4] = {r[1][0], r[1][1], r[1][2], r[1][3]};
	float rline3[4] = {r[2][0], r[2][1], r[2][2], r[2][3]};
	float rline4[4] = {r[3][0], r[3][1], r[3][2], r[3][3]};

	if (ImGui::DragFloat4("r1", rline1)){
	}
	if (ImGui::DragFloat4("r2", rline2)){
	}
	if (ImGui::DragFloat4("r3", rline3)){
	}
	if (ImGui::DragFloat4("r4", rline4)){
	}

	float rX = -glm::degrees(glm::atan(r[2][1],r[2][2]));
	float rY = -glm::degrees(glm::atan(-r[2][0],glm::sqrt(r[2][1]*r[2][1] + r[2][2]*r[2][2])));
	float rZ = -glm::degrees(glm::atan(r[1][0],r[0][0]));

	auto& view = component->getViewMatrix();
	float viewline1[4] = {view[0][0], view[0][1], view[0][2], view[0][3]};
	float viewline2[4] = {view[1][0], view[1][1], view[1][2], view[1][3]};
	float viewline3[4] = {view[2][0], view[2][1], view[2][2], view[2][3]};
	float viewline4[4] = {view[3][0], view[3][1], view[3][2], r[3][3]};

	if (ImGui::DragFloat4("view1", viewline1)){
	}
	if (ImGui::DragFloat4("view1", viewline2)){
	}
	if (ImGui::DragFloat4("view1", viewline3)){
	}
	if (ImGui::DragFloat4("view1", viewline4)){
	}

	float globalRot[] = {rX, rY, rZ};
	if (ImGui::DragFloat3("RotateGlobal", globalRot)) {
		component->setRotate({globalRot[0], globalRot[1], globalRot[2]});
	}

}

void ComponentsDebug::lodComponentInternal(LodComponent* component) {
	if (!component) {
		return;
	}

	auto& lodLevels = const_cast<std::vector<float>&>(component->getLodLevelValues());
	int i = 0;

	bool needSort = false;
	for (auto& level : lodLevels) {
		std::string levelId = "level" + std::to_string(i);
		if (ImGui::DragFloat(levelId.c_str(), &level, 0.001f)) {
			needSort = true;
		}
		i++;
	}

	if (needSort) {
		std::ranges::sort(lodLevels);
		std::ranges::reverse(lodLevels);
	}

	if (component->getLodType() == eLodType::DISTANCE) {
		ImGui::Text("Type: %s", "DISTANCE");
	}
	else if (component->getLodType() == eLodType::SCREEN_SPACE) {
		ImGui::Text("Type: %s", "SCREEN_SPACE");
	}

	ImGui::Text("%zu", component->getLodLevel());
	ImGui::Text("%f", component->getCurrentLodValue());
}

void ComponentsDebug::modelComponentInternal(ComponentsModule::ModelComponent* component) {
	
}
