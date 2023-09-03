#include "Projection.h"

#include <ext/matrix_clip_space.hpp>

using namespace Engine::ProjectionModule;

Projection::Projection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) : mNear(zNear), mFar(zFar), mLeftBtm(leftBtm), mRightTop(rightTop) {
	mType = eProjectionType::ORTHO;
	initProjection();
	
}

Projection::Projection(float FOV, float aspect, float zNear, float zFar) : mNear(zNear), mFar(zFar), mFOV(FOV), mAspect(aspect) {
	mType = eProjectionType::PERSPECTIVE;
	initProjection();
}

const glm::mat4& Projection::getProjectionsMatrix() const {
	return mProjectionMatrix;
}

void Projection::initProjection() {
	switch(mType) {
	case ORTHO:
		mProjectionMatrix = glm::ortho(mLeftBtm.x, mRightTop.x, mLeftBtm.y, mRightTop.y, mNear, mFar);
		break;
	case PERSPECTIVE:
		mProjectionMatrix = glm::perspective(glm::radians(mFOV), mAspect, mNear, mFar);
		break;
	case NONE:
		break;
	}
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


void Projection::setLeftBtm(glm::vec2 point) {
	assert(mType == ORTHO);
	if (mLeftBtm == point) {
		return;
	}

	mLeftBtm = point;
	initProjection();

}
void Projection::setRightTop(glm::vec2 point) {
	assert(mType == ORTHO);
	if (mRightTop == point) {
		return;
	}

	mRightTop = point;
	initProjection();
}

void Projection::setProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) {
	mType = ORTHO;
	mLeftBtm = leftBtm;
	mRightTop = rightTop;
	mNear = zNear;
	mFar = zFar;
	initProjection();
}

void Projection::setFOV(float FOV) {
	assert(mType == PERSPECTIVE);

	if (std::fabs(mFOV - FOV) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mFOV = FOV;
	initProjection();
}

float Projection::getFOV() const {
	assert(mType == PERSPECTIVE);
	return mFOV;
}

void Projection::setAspect(float aspect) {
	assert(mType == PERSPECTIVE);
	if (std::fabs(mAspect - aspect) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	mAspect = aspect;
	initProjection();
}

float Projection::getAspect() const {
	assert(mType == PERSPECTIVE);
	return mAspect;
}

void Projection::setProjection(float FOV, float aspect, float zNear, float zFar) {
	mType = PERSPECTIVE;
	mFOV = FOV;
	mAspect = aspect;
	mNear = zNear;
	mFar = zFar;
	initProjection();
}