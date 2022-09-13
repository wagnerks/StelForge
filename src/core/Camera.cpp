#include "Camera.h"

#include "Engine.h"
#include "componentsModule/ProjectionComponent.h"
#include "componentsModule/TransformComponent.h"

Camera::Camera(size_t entID, GameEngine::ProjectionModule::Projection view, glm::vec3 position, float yaw) : Entity<Camera>(entID) {
	auto tc = addComponent<TransformComponent>();
	tc->setPos(position);
	tc->setRotate({0.f, yaw, 0.f});

	tc->reloadTransform();
	auto projectionComp = addComponent<ProjectionComponent>();
	projectionComp->initProjection(view);

	setNodeId("mainCamera");
}


void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
	auto tc = getComponent<TransformComponent>();

	const float velocity = MovementSpeed * deltaTime;

	glm::vec3 dif = {};
	if (direction == FORWARD)
		dif += tc->getForward() * velocity;
	if (direction == BACKWARD)
		dif -= tc->getForward() * velocity;
	if (direction == LEFT)
		dif -= tc->getRight() * velocity;
	if (direction == RIGHT)
		dif += tc->getRight() * velocity;
	if (direction == TOP)
		dif += glm::vec3{0.f, 1.f, 0.f} * velocity;
	if (direction == BOTTOM)
		dif -= glm::vec3{0.f, 1.f, 0.f} * velocity;

	tc->setPos(tc->getPos() + dif);
	tc->reloadTransform();
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
	
	tc->setRotate({Pitch, Yaw, 0.f});
	tc->reloadTransform();
}
void Camera::ProcessMouseScroll(float yoffset) {
	auto fov = getComponent<ProjectionComponent>()->getProjection().getFOV();
	fov -= yoffset;

	getComponent<ProjectionComponent>()->getProjection().setFOV(std::min(std::max(1.f, fov), 90.f));
}