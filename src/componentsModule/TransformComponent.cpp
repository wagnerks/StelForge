#include "TransformComponent.h"

#include <ext/matrix_transform.hpp>
#include "mat4x4.hpp"
#include "core/Node.h"


using namespace GameEngine::ComponentsModule;

void TransformComponent::updateComponent() {
	reloadTransform();
}

const glm::vec3& TransformComponent::getPos() {
	return pos;
}

void TransformComponent::setX(float x) {
	dirty = dirty || std::fabs(x - pos.x) > std::numeric_limits<float>::epsilon();

	pos.x = x;
}
void TransformComponent::setY(float y) {
	dirty = dirty || std::fabs(y - pos.y) > std::numeric_limits<float>::epsilon();

	pos.y = y;
}
void TransformComponent::setZ(float z) {
	dirty = dirty || std::fabs(z - pos.z) > std::numeric_limits<float>::epsilon();

	pos.z = z;
}
void TransformComponent::setPos(glm::vec3 pos) {
	dirty = dirty || this->pos != pos;

	this->pos = pos;
}

const glm::vec3& TransformComponent::getRotate() {
	return rotate;
}

void TransformComponent::setRotateX(float x) {
	dirty = dirty || std::fabs(x - rotate.x) > std::numeric_limits<float>::epsilon();

	rotate.x = x;
}
void TransformComponent::setRotateY(float y) {
	dirty = dirty || std::fabs(y - rotate.y) > std::numeric_limits<float>::epsilon();

	rotate.y = y;
}
void TransformComponent::setRotateZ(float z) {
	dirty = dirty || std::fabs(z - rotate.z) > std::numeric_limits<float>::epsilon();

	rotate.z = z;
}
void TransformComponent::setRotate(glm::vec3 rotate) {
	dirty = dirty || this->rotate != rotate;

	this->rotate = rotate;
}

const glm::vec3& TransformComponent::getScale() {
	return scale;
}

void TransformComponent::setScaleX(float x) {
	dirty = dirty || std::fabs(x - scale.x) > std::numeric_limits<float>::epsilon();

	scale.x = x;
}
void TransformComponent::setScaleY(float y) {
	dirty = dirty || std::fabs(y - scale.y) > std::numeric_limits<float>::epsilon();

	scale.y = y;
}
void TransformComponent::setScaleZ(float z) {
	dirty = dirty || std::fabs(z - scale.z) > std::numeric_limits<float>::epsilon();

	scale.z = z;
}
void TransformComponent::setScale(glm::vec3 scale) {
	dirty = dirty || this->scale != scale;

	this->scale = scale;
}
const glm::mat4& TransformComponent::getTransform() {
	return transform;
}

void TransformComponent::reloadTransform() {
	if (!dirty) {
		return;
	}
	dirty = false;

	if (ownerNode && ownerNode->getParent()) {
		transform = ownerNode->getParent()->getComponent<TransformComponent>()->getTransform() * getLocalTransform();
	}
	else {
		transform = getLocalTransform();	
	}

	if (ownerNode) {
		for (auto node : ownerNode->getElements()) {
			const auto childTransform = node->getComponent<TransformComponent>();
			childTransform->markDirty();
			childTransform->reloadTransform();
		}
	}
	
}

glm::mat4 TransformComponent::getLocalTransform() {
    const glm::mat4 transformX = glm::rotate(glm::mat4(1.0f), glm::radians(rotate.x), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::mat4 transformY = glm::rotate(glm::mat4(1.0f), glm::radians(rotate.y), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 transformZ = glm::rotate(glm::mat4(1.0f), glm::radians(rotate.z), glm::vec3(0.0f, 0.0f, 1.0f));

    const glm::mat4 rotationMatrix = transformY * transformX * transformZ;

    return glm::translate(glm::mat4(1.0f), pos) * rotationMatrix * glm::scale(glm::mat4(1.0f), scale);
}

void TransformComponent::setOwnerNode(GameEngine::CoreModule::Node* node) {
	ownerNode = node;
}

void TransformComponent::markDirty() {
	dirty = true;
}