#include "Projection.h"

#include <ext/matrix_clip_space.hpp>

using namespace Engine::ProjectionModule;

OrthoProjection::OrthoProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) : Projection(zNear, zFar), mLeftBtm(leftBtm), mRightTop(rightTop) {
	OrthoProjection::initProjection();
}

PerspectiveProjection::PerspectiveProjection(float FOV, float aspect, float zNear, float zFar) : Projection(zNear, zFar), mFOV(FOV), mAspect(aspect) {
	PerspectiveProjection::initProjection();
}

const glm::mat4& Projection::getProjectionsMatrix() const {
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


void OrthoProjection::setLeftBtm(glm::vec2 point) {
	if (mLeftBtm == point) {
		return;
	}

	mLeftBtm = point;
	initProjection();

}
void OrthoProjection::setRightTop(glm::vec2 point) {
	if (mRightTop == point) {
		return;
	}

	mRightTop = point;
	initProjection();
}

void OrthoProjection::initProjection() {
	mProjectionMatrix = glm::ortho(mLeftBtm.x, mRightTop.x, mLeftBtm.y, mRightTop.y, getNear(), getFar());
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
	mProjectionMatrix = glm::perspective(glm::radians(mFOV), mAspect, getNear(), getFar());
}