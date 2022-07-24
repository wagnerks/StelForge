#include "Camera.h"

#include "Engine.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch): Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED),
                                                                          MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
	Position = position;
	WorldUp = up;
	Yaw = yaw;
	Pitch = pitch;
	updateCameraVectors();
}

Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch):
	Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM) {
	Position = glm::vec3(posX, posY, posZ);
	WorldUp = glm::vec3(upX, upY, upZ);
	Yaw = yaw;
	Pitch = pitch;
	updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() {
	return glm::lookAt(Position, Position + Front, Up);
}
void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
	float velocity = MovementSpeed * deltaTime;
	if (direction == FORWARD)
		Position += Front * velocity;
	if (direction == BACKWARD)
		Position -= Front * velocity;
	if (direction == LEFT)
		Position -= Right * velocity;
	if (direction == RIGHT)
		Position += Right * velocity;
	if (direction == TOP)
		Position += glm::vec3{0.f, 1.f, 0.f} * velocity;
	if (direction == BOTTOM)
		Position -= glm::vec3{0.f, 1.f, 0.f} * velocity;
}
void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
	if (!processMouse) {
		return;
	}

	xoffset *= MouseSensitivity;
	yoffset *= MouseSensitivity;

	Yaw += xoffset;
	Pitch += yoffset;

	// make sure that when pitch is out of bounds, screen doesn't get flipped
	if (constrainPitch) {
		if (Pitch > 89.0f)
			Pitch = 89.0f;
		if (Pitch < -89.0f)
			Pitch = -89.0f;
	}

	// update Front, Right and Up Vectors using the updated Euler angles
	updateCameraVectors();
}
void Camera::ProcessMouseScroll(float yoffset) {
	Zoom -= (float)yoffset;
	if (Zoom < 1.0f)
		Zoom = 1.0f;
	if (Zoom > 45.0f)
		Zoom = 45.0f;
}
glm::mat4 Camera::getProjectionsMatrix() {
	auto eng = GameEngine::Engine::getInstance();
	projection = glm::perspective(glm::radians(Zoom), static_cast<float>(eng->getRenderer()->SCR_WIDTH) / static_cast<float>(eng->getRenderer()->SCR_HEIGHT), 0.1f, 100.0f);

	return projection;
}
