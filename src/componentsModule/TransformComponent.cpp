#include "TransformComponent.h"

#include <detail/type_quat.hpp>
#include <ext/matrix_transform.hpp>
#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>

#include "TreeComponent.h"
#include "core/ThreadPool.h"
#include "mathModule/MathUtils.h"
#include "propertiesModule/PropertiesSystem.h"
#include "propertiesModule/TypeName.h"

using namespace Engine::ComponentsModule;

const glm::vec3& TransformComponent::getPos(bool global) const {
	if (global) {
		return globalPos;
	}
	return pos;
}

void TransformComponent::setX(float x) {
	if (std::fabs(x - pos.x) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	pos.x = x;
}
void TransformComponent::setY(float y) {
	if (std::fabs(y - pos.y) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	pos.y = y;
}
void TransformComponent::setZ(float z) {
	if (std::fabs(z - pos.z) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	pos.z = z;
}
void TransformComponent::setPos(const glm::vec3& pos) {
	if (this->pos != pos) {
		markDirty();
	}
	this->pos = pos;
}

//x - pitch, y - yaw, z - roll
const glm::vec3& TransformComponent::getRotate() const {
	return rotate;
}

void TransformComponent::setRotateX(float x) {
	if (std::fabs(x - rotate.x) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	rotate.x = x;
	rotateQuat = glm::quat({ glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z) });
}
void TransformComponent::setRotateY(float y) {
	if (std::fabs(y - rotate.y) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	rotate.y = y;
	rotateQuat = glm::quat({ glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z) });
}
void TransformComponent::setRotateZ(float z) {
	if (std::fabs(z - rotate.z) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	rotate.z = z;
	rotateQuat = glm::quat({ glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z) });
}
void TransformComponent::setRotate(const glm::vec3& rotate) {
	if (this->rotate != rotate) {
		markDirty();
	}
	this->rotate = rotate;
	rotateQuat = glm::quat({ glm::radians(rotate.x), glm::radians(rotate.y),glm::radians(rotate.z) });
}

const glm::vec3& TransformComponent::getScale(bool global) const {
	if (global) {
		return globalScale;
	}

	return scale;
}

void TransformComponent::setScaleX(float x) {
	if (std::fabs(x - scale.x) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}

	scale.x = x;
}
void TransformComponent::setScaleY(float y) {
	if (std::fabs(y - scale.y) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	scale.y = y;
}
void TransformComponent::setScaleZ(float z) {
	if (std::fabs(z - scale.z) > std::numeric_limits<float>::epsilon()) {
		markDirty();
	}
	scale.z = z;
}
void TransformComponent::setScale(const glm::vec3& scale) {
	if (this->scale != scale) {
		markDirty();
	}

	this->scale = scale;
}
const glm::mat4& TransformComponent::getTransform() const {
	return transform;
}

void TransformComponent::setTransform(const glm::mat4& transform) {
	this->transform = transform;
}

glm::mat4 TransformComponent::getRotationMatrix() const {
	return glm::toMat4(rotateQuat);
}

glm::quat TransformComponent::getRotationMatrixQuaternion() const {
	return rotateQuat;
}

void TransformComponent::reloadTransform() {
	/*if (!dirty) {
		return;
	}*/
	dirty = false;
	transform = getLocalTransform();

	auto tree = ECSHandler::registry()->getComponent<TreeComponent>(getEntityId());

	if (tree) {
		if (auto parentTransform = ECSHandler::registry()->getComponent<TransformComponent>(tree->getParent())) {
			if (parentTransform->isDirty()) {
				parentTransform->reloadTransform();
			}
			
			transform = parentTransform->getTransform() * transform;
		}

	}

	view = glm::inverse(transform);
	globalScale = calculateGlobalScale();
	globalPos = glm::vec3(transform[3]);

	if (tree) {
		for (const auto childTransform : tree->getChildren()) {
			if (auto transformPtr = ECSHandler::registry()->getComponent<TransformComponent>(childTransform)) {
				//transformPtr->markDirty();
				transformPtr->reloadTransform();
			}
		}
	}
	
}

glm::mat4 TransformComponent::getLocalTransform() const {
	return glm::translate(glm::mat4(1.0f), pos) * getRotationMatrix() * glm::scale(glm::mat4(1.0f), scale);
}

const glm::mat4& TransformComponent::getViewMatrix() const {
	return view;
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
	ECSHandler::registry()->addComponent<DirtyTransform>(getEntityId());

	auto tree = ECSHandler::registry()->getComponent<TreeComponent>(getEntityId());
	if (tree) {
		for (const auto childTransform : tree->getChildren()) {
			if (auto transformPtr = ECSHandler::registry()->getComponent<TransformComponent>(childTransform)) {
				transformPtr->markDirty();
			}
		}
	}
	dirty = true;
}

bool TransformComponent::isDirty() const {
	return dirty;
}

void TransformComponent::serialize(Json::Value& data) {

	data["Scale"].append(scale.x);
	data["Scale"].append(scale.y);
	data["Scale"].append(scale.z);

	data["Pos"].append(pos.x);
	data["Pos"].append(pos.y);
	data["Pos"].append(pos.z);

	data["Rotate"].append(rotate.x);
	data["Rotate"].append(rotate.y);
	data["Rotate"].append(rotate.z);
}

void TransformComponent::deserialize(const Json::Value& data) {
	if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Scale")) {
		setScale(PropertiesModule::JsonUtils::getVec3(*val));
	}

	if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Pos")) {
		setPos(PropertiesModule::JsonUtils::getVec3(*val));
	}

	if (auto val = PropertiesModule::JsonUtils::getValueArray(data, "Rotate")) {
		setRotate(PropertiesModule::JsonUtils::getVec3(*val));
	}
}

TransformComponent::~TransformComponent() {
	
}

glm::vec3 TransformComponent::calculateGlobalScale() {
	auto& t = transform;

	return {
		sqrt(t[0][0] * t[0][0] + t[0][1] * t[0][1] + t[0][2] * t[0][2]),
		sqrt(t[1][0] * t[1][0] + t[1][1] * t[1][1] + t[1][2] * t[1][2]),
		sqrt(t[2][0] * t[2][0] + t[2][1] * t[2][1] + t[2][2] * t[2][2])
	};
}