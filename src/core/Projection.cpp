#include "Projection.h"

#include "mathModule/Utils.h"

using namespace SFE::ProjectionModule;

OrthoProjection::OrthoProjection(Math::Vec2 leftBtm, Math::Vec2 rightTop, float zNear, float zFar) : Projection(zNear, zFar), mLeftBtm(leftBtm), mRightTop(rightTop) {
	OrthoProjection::initProjection();
}

PerspectiveProjection::PerspectiveProjection(float FOV, float aspect, float zNear, float zFar) : Projection(zNear, zFar), mFOV(FOV), mAspect(aspect) {
	PerspectiveProjection::initProjection();
}

const SFE::Math::Mat4& Projection::getProjectionsMatrix() const {
	return mProjectionMatrix;
}

float Projection::getNear() const {
	return mNear;
}

float Projection::getFar() const {
	return mFar;
}

void Projection::setNear(float near) {
	if (std::fabs(mNear - near) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mNear = near;
	initProjection();
}
void Projection::setFar(float far) {
	if (std::fabs(mFar - far) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mFar = far;
	initProjection();
}

void Projection::setNearFar(float near, float far) {
	if (std::fabs(mFar - far) < std::numeric_limits<float>::epsilon() && std::fabs(mNear - near) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mNear = near;
	mFar = far;

	initProjection();
}


void OrthoProjection::setLeftBtm(Math::Vec2 point) {
	if (mLeftBtm == point) {
		return;
	}

	mLeftBtm = point;
	initProjection();

}
void OrthoProjection::setRightTop(Math::Vec2 point) {
	if (mRightTop == point) {
		return;
	}

	mRightTop = point;
	initProjection();
}

void OrthoProjection::initProjection() {
	mProjectionMatrix = Math::orthoRH_NO(mLeftBtm.x, mRightTop.x, mLeftBtm.y, mRightTop.y, getNear(), getFar());
}

void PerspectiveProjection::setFOV(float FOV) {
	if (std::fabs(mFOV - FOV) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mFOV = FOV;
	initProjection();
}

float PerspectiveProjection::getFOV() const {
	return mFOV;
}

void PerspectiveProjection::setAspect(float aspect) {
	if (std::fabs(mAspect - aspect) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mAspect = aspect;
	initProjection();
}

float PerspectiveProjection::getAspect() const {
	return mAspect;
}

void PerspectiveProjection::initProjection() {
	mProjectionMatrix = Math::perspectiveRH_NO(Math::radians(mFOV), mAspect, getNear(), getFar());
}