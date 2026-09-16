#include "FrustumComponent.h"

void FrustumComponent::updateFrustum(const MathModule::Projection& projection, const Math::Mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection.getProjectionsMatrix() * view);
}

void FrustumComponent::updateFrustum(const Math::Mat4& projView) {
	mFrustum = FrustumModule::createFrustum(projView);
}

void FrustumComponent::updateFrustum(const Math::Mat4& projection, const Math::Mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection * view);
}



SFE::FrustumModule::Frustum* FrustumComponent::getFrustum() {
	return &mFrustum;
}