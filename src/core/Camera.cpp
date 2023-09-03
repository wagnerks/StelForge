#include "Camera.h"

#include "Engine.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"

Camera::Camera(size_t entID, float FOV, float aspect, float zNear, float zFar, glm::vec3 position, float yaw) : Entity(entID) {
	auto tc = addComponent<TransformComponent>();
	tc->setPos(position);
	tc->setRotate({ 0.f, yaw, 0.f });

	tc->reloadTransform();

	addComponent<CameraComponent>(FOV, aspect, zNear, zFar);

	setNodeId("mainCamera");
}


void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
	if (!processMouse) {
		return;
	}
	xoffset *= MouseSensitivity;
	yoffset *= MouseSensitivity;
	auto tc = getComponent<TransformComponent>();
	auto Pitch = tc->getRotate().x;
	auto Yaw = tc->getRotate().y;
	Yaw -= xoffset;

	Pitch += yoffset;

	if (constrainPitch) {
		Pitch = std::min(std::max(Pitch, -90.f), 90.f);
	}

	tc->setRotate({ Pitch, Yaw, 0.f });
	tc->reloadTransform();
}
void Camera::ProcessMouseScroll(float yoffset) {
	auto fov = getComponent<CameraComponent>()->getProjection().getFOV();
	fov -= yoffset;

	getComponent<CameraComponent>()->getProjection().setFOV(std::min(std::max(1.f, fov), 90.f));
}