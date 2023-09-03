#pragma once
#include <glad/glad.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "Projection.h"
#include "ecsModule/EntityBase.h"


enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	TOP,
	BOTTOM
};

class Camera : public ecsModule::Entity<Camera> {
public:
	Camera(size_t entID, Engine::ProjectionModule::Projection view, glm::vec3 position = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = 0.f);

	float MovementSpeed = 10.f;
	float MouseSensitivity = 0.1f;
	
	bool processMouse = false;
	void ProcessKeyboard(Camera_Movement direction, float deltaTime);
	void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true);
	void ProcessMouseScroll(float yoffset);
};
