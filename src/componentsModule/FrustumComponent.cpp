#include "FrustumComponent.h"

void FrustumComponent::updateFrustum(const ProjectionModule::Projection& projection, const Math::Mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection.getProjectionsMatrix() * view);
}

void FrustumComponent::updateFrustum(const Math::Mat4& projView) {
	mFrustum = FrustumModule::createFrustum(projView);
}

void FrustumComponent::updateFrustum(const Math::Mat4& projection, const Math::Mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection * view);
}

bool FrustumComponent::isOnFrustum(const FrustumModule::BoundingVolume& bVolume) const {
	return bVolume.isOnFrustum(mFrustum);
}

SFE::FrustumModule::Frustum* FrustumComponent::getFrustum() {
	return &mFrustum;
}