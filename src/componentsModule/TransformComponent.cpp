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

void TransformComponent::updateComponent() {
	reloadTransform();
}

void TransformComponent::setWithView(bool isWithView) {
	dirty = isWithView != withView;
	
	withView = isWithView;
}
bool TransformComponent::isWithView() {
	return withView;
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

//x - pitch, y - yaw, z - roll
const glm::vec3& TransformComponent::getRotate() {
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
glm::mat4& TransformComponent::getTransform() {
	return transform;
}

glm::mat4 TransformComponent::getRotationMatrix() {
	return glm::toMat4(rotateQuat);
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

	if (withView) {
		viewMatrix = glm::lookAt(getPos(), getPos() + getFront(), {0.f,1.f,0.f});
	}
	else {
		viewMatrix = {};
	}
}

glm::mat4 TransformComponent::getLocalTransform() {
    return glm::translate(glm::mat4(1.0f), pos) * getRotationMatrix() * glm::scale(glm::mat4(1.0f), scale);
}

const glm::mat4& TransformComponent::getViewMatrix() const {
	return viewMatrix;
}

glm::vec3 TransformComponent::getFront() {
	glm::vec3 front;

	front.z = cos(glm::radians(getRotate().y)) * cos(glm::radians(getRotate().x));
	front.y = -sin(glm::radians(getRotate().x));
	front.x = sin(glm::radians(getRotate().y)) * cos(glm::radians(getRotate().x));

	return glm::normalize(front);
}

void TransformComponent::markDirty() {
	dirty = true;
}
