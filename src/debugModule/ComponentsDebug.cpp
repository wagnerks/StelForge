#include "ComponentsDebug.h"

#include "imgui.h"

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
