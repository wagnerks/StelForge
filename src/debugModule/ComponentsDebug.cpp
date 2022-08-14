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
			if (ImGui::Selectable(std::to_string(id).c_str(), selected)) {
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
