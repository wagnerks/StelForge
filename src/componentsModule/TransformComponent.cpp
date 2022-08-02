#include "TransformComponent.h"

#include <detail/type_quat.hpp>
#include <ext/matrix_transform.hpp>
#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>
#include "mat4x4.hpp"
#include "nodeModule/Node.h"


using namespace GameEngine::ComponentsModule;

TransformComponent::TransformComponent(ComponentHolder* holder): Component(holder), ownerNode(dynamic_cast<NodeModule::Node*>(holder)) {
}

TransformComponent::~TransformComponent() {
	for (auto childTransform : childTransforms) {
		childTransform->parentTransform = nullptr;
	}
}

void TransformComponent::updateComponent() {
	reloadTransform();
}

void TransformComponent::addChildTransform(TransformComponent* child) {
	if (!child) {
		return;
	}

	if (child->parentTransform) {
		child->parentTransform->removeChildTransform(child);
		child->parentTransform = nullptr;
	}

	childTransforms.emplace_back(child);
	child->parentTransform = this;
}

void TransformComponent::removeChildTransform(TransformComponent* child) {
	if (!child) {
		return;
	}

	std::erase(childTransforms, child);
}

glm::vec3 TransformComponent::getPos(bool global) const {
	if (global) {
		return transform[3];
	}
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

//x - pitch, y - yaw, z - roll
const glm::vec3& TransformComponent::getRotate() const {
	return rotate;
}

void TransformComponent::setRotateX(float x) {
	dirty = dirty || std::fabs(x - rotate.x) > std::numeric_limits<float>::epsilon();

	rotate.x = x;
	rotateQuat = glm::quat({glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z)});
}
void TransformComponent::setRotateY(float y) {
	dirty = dirty || std::fabs(y - rotate.y) > std::numeric_limits<float>::epsilon();

	rotate.y = y;
	rotateQuat = glm::quat({glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z)});
}
void TransformComponent::setRotateZ(float z) {
	dirty = dirty || std::fabs(z - rotate.z) > std::numeric_limits<float>::epsilon();

	rotate.z = z;
	rotateQuat = glm::quat({glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z)});
}
void TransformComponent::setRotate(glm::vec3 rotate) {
	dirty = dirty || this->rotate != rotate;

	this->rotate = rotate;
	rotateQuat = glm::quat({glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z)});
}

const glm::vec3& TransformComponent::getScale() const {
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
const glm::mat4& TransformComponent::getTransform() const {
	return transform;
}

glm::mat4 TransformComponent::getRotationMatrix() const {
	return glm::toMat4(rotateQuat);
}

void TransformComponent::reloadTransform() {
	if (!dirty) {
		return;
	}
	dirty = false;

	transform = getLocalTransform();

	if (parentTransform) {
		parentTransform->reloadTransform();
		transform = parentTransform->getTransform() * transform;
	}

	if (ownerNode) {
		for (auto node : ownerNode->getElements()) {
			const auto childTransform = node->getComponent<TransformComponent>();
			childTransform->markDirty();
			childTransform->reloadTransform();
		}
	}

	for (auto childTransform : childTransforms) {
		childTransform->markDirty();
		childTransform->reloadTransform();
	}
}

glm::mat4 TransformComponent::getLocalTransform() const {
    return glm::translate(glm::mat4(1.0f), pos) * getRotationMatrix() * glm::scale(glm::mat4(1.0f), scale);
}

glm::mat4 TransformComponent::getViewMatrix() const {
	return glm::inverse(transform);
}

glm::vec3 TransformComponent::getRight() {
	return transform[0];
}

glm::vec3 TransformComponent::getUp() {
	return transform[1];
}

glm::vec3 TransformComponent::getBackward() {
	return transform[2];
}

glm::vec3 TransformComponent::getForward() {
	return -transform[2];
}

void TransformComponent::markDirty() {
	dirty = true;
}
