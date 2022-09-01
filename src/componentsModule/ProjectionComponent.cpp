#include "ProjectionComponent.h"

ProjectionComponent::ProjectionComponent() {
}

GameEngine::ProjectionModule::Projection& ProjectionComponent::getProjection() {
	return mProjection;
}

void ProjectionComponent::initProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) {
	mProjection.setProjection(leftBtm, rightTop, zNear, zFar);
}
void ProjectionComponent::initProjection(float FOV, float aspect, float zNear, float zFar) {
	mProjection.setProjection(FOV, aspect, zNear, zFar);
}

void ProjectionComponent::initProjection(const ProjectionModule::Projection& projection) {
	mProjection = projection;
}
