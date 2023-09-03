#include "CameraComponent.h"

CameraComponent::CameraComponent() {
}

CameraComponent::CameraComponent(float FOV, float aspect, float zNear, float zFar) {
	initProjection(FOV, aspect, zNear, zFar);
}

Engine::ProjectionModule::PerspectiveProjection& CameraComponent::getProjection() {
	return mProjection;
}

void CameraComponent::initProjection(float FOV, float aspect, float zNear, float zFar) {
	mProjection = Engine::ProjectionModule::PerspectiveProjection(FOV, aspect, zNear, zFar);
}

void CameraComponent::initProjection(const Engine::ProjectionModule::PerspectiveProjection& projection) {
	mProjection = projection;
}
