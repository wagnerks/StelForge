#include "Projection.h"

#include <ext/matrix_clip_space.hpp>

using namespace GameEngine::ProjectionModule;

const glm::mat4& Projection::getProjectionsMatrix() const {
	return projection;
}

float Projection::getZNear() {
	return zNear;
}

float Projection::getZFar() {
	return zFar;
}

void Projection::setZNear(float near) {
	if (std::fabs(zNear - near) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	zNear = near;
	initProjection();
}
void Projection::setZFar(float far) {
	if (std::fabs(zFar - far) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	zFar = far;
	initProjection();
}

OrthoProjection::OrthoProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) : Projection(zNear, zFar), leftBtm(leftBtm), rightTop(rightTop) {
	OrthoProjection::initProjection();
}

void OrthoProjection::initProjection() {
	projection = glm::ortho(leftBtm.x, rightTop.x, leftBtm.y, rightTop.y, zNear, zFar);
}

void OrthoProjection::setLeftBtm(glm::vec2 point) {
	if (leftBtm == point) {
		return;
	}

	leftBtm = point;
	initProjection();

}
void OrthoProjection::setRightTop(glm::vec2 point) {
	if (rightTop == point) {
		return;
	}

	rightTop = point;
	initProjection();
}

void OrthoProjection::setProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar) {
	this->leftBtm = leftBtm;
	this->rightTop = rightTop;
	this->zNear = zNear;
	this->zFar = zFar;
	initProjection();
}

PerspectiveProjection::PerspectiveProjection(float FOV, float aspect, float zNear, float zFar) : Projection(zNear, zFar), FOV(FOV), aspect(aspect) {
	PerspectiveProjection::initProjection();
}

void PerspectiveProjection::setFOV(float aFOV) {
	if (std::fabs(FOV - aFOV) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	FOV = aFOV;
	initProjection();
}

float PerspectiveProjection::getFOV() const {
	return FOV;
}

void PerspectiveProjection::setAspect(float anAspect) {
	if (std::fabs(aspect - anAspect) < std::numeric_limits<float>::epsilon()) {
		return;
	}
	aspect = anAspect;
	initProjection();
}

void PerspectiveProjection::initProjection() {
	projection = glm::perspective(glm::radians(FOV), aspect, zNear, zFar);
}

void PerspectiveProjection::setProjection(float FOV, float aspect, float zNear, float zFar) {
	this->FOV = FOV;
	this->aspect = aspect;
	this->zNear = zNear;
	this->zFar = zFar;
	initProjection();
}

float PerspectiveProjection::getAspect() const {
	return aspect;
}
