#include "FrustumComponent.h"

FrustumComponent::FrustumComponent() {}

void FrustumComponent::updateFrustum(const ProjectionModule::Projection& projection, const glm::mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection.getProjectionsMatrix() * view);
}

void FrustumComponent::updateFrustum(const glm::mat4& projView) {
	mFrustum = FrustumModule::createFrustum(projView);
}

void FrustumComponent::updateFrustum(const glm::mat4& projection, const glm::mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection * view);
}

bool FrustumComponent::isOnFrustum(const FrustumModule::BoundingVolume& bVolume) const {
	return bVolume.isOnFrustum(mFrustum);
}

Engine::FrustumModule::Frustum* FrustumComponent::getFrustum() {
	return &mFrustum;
}