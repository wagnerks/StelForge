#include "LightComponent.h"

LightComponent::LightComponent(eLightType type) : mType(type) {
}

Engine::ComponentsModule::eLightType LightComponent::getType() const {
	return mType;
}

void LightComponent::setBias(float bias) {
	mBias = bias;
}

float LightComponent::getBias() const {
	return mBias;
}

void LightComponent::setIntensity(float intensity) {
	mIntensity = intensity;
}

float LightComponent::getIntensity() const {
	return mIntensity;
}

void LightComponent::setTexelSize(const glm::vec2& texelSize) {
	mTexelSize = texelSize;
}

const glm::vec2& LightComponent::getTexelSize() const {
	return mTexelSize;
}

void LightComponent::setSamples(int samples) {
	mSamples = samples;
}

int LightComponent::getSamples() const {
	return mSamples;
}

void LightComponent::setLightColor(const glm::vec3& lightColor) {
	mLightColor = lightColor;
}

const glm::vec3& LightComponent::getLightColor() const {
	return mLightColor;
}
