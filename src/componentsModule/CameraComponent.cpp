#include "CameraComponent.h"

#include <vector>

#include "TransformComponent.h"
#include "core/ECSHandler.h"

CameraComponent::CameraComponent(float FOV, float aspect, float zNear, float zFar){
	initProjection(FOV, aspect, zNear, zFar);
}

SFE::MathModule::PerspectiveProjection& CameraComponent::getProjection() {
	return mProjection;
}

void CameraComponent::initProjection(const float FOV, float aspect, float zNear, float zFar) {
	mProjection = SFE::MathModule::PerspectiveProjection(FOV, aspect, zNear, zFar);
}

void CameraComponent::initProjection(const SFE::MathModule::PerspectiveProjection& projection) {
	mProjection = projection;
}

void CameraComponent::updateFrustum(const Math::Mat4& view) const {
	mFrustum = FrustumModule::createFrustum(mProjection.getProjectionsMatrix() * view);
}

const SFE::FrustumModule::Frustum& CameraComponent::getFrustum() const {
	return mFrustum;
}