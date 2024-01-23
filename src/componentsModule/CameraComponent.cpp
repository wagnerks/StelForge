#include "CameraComponent.h"

#include <vector>

CameraComponent::CameraComponent(ecss::SectorId id) : ComponentInterface(id) {
}

CameraComponent::CameraComponent(ecss::SectorId id, float FOV, float aspect, float zNear, float zFar) : ComponentInterface(id) {
	initProjection(FOV, aspect, zNear, zFar);
}

SFE::ProjectionModule::PerspectiveProjection& CameraComponent::getProjection() {
	return mProjection;
}

void CameraComponent::initProjection(float FOV, float aspect, float zNear, float zFar) {
	mProjection = SFE::ProjectionModule::PerspectiveProjection(FOV, aspect, zNear, zFar);
}

void CameraComponent::initProjection(const SFE::ProjectionModule::PerspectiveProjection& projection) {
	mProjection = projection;
}

void CameraComponent::updateFrustum(const Math::Mat4& view) {
	mFrustum = FrustumModule::createFrustum(mProjection.getProjectionsMatrix() * view);
}

const SFE::FrustumModule::Frustum& CameraComponent::getFrustum() const {
	return mFrustum;
}