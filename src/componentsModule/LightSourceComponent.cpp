#include "LightSourceComponent.h"

LightSourceComponent::LightSourceComponent(eLightType type) : mType(type) {
}

Engine::ComponentsModule::eLightType LightSourceComponent::getType() const {
	return mType;
}



void LightSourceComponent::setIntensity(float intensity) {
	mIntensity = intensity;
}

float LightSourceComponent::getIntensity() const {
	return mIntensity;
}


void LightSourceComponent::setLightColor(const glm::vec3& lightColor) {
	mLightColor = lightColor;
}

const glm::vec3& LightSourceComponent::getLightColor() const {
	return mLightColor;
}

void LightSourceComponent::setBias(float bias) {
	mBias = bias;
}

float LightSourceComponent::getBias() const {
	return mBias;
}

void LightSourceComponent::setTexelSize(const glm::vec2& texelSize) {
	mTexelSize = texelSize;
}

const glm::vec2& LightSourceComponent::getTexelSize() const {
	return mTexelSize;
}

void LightSourceComponent::setSamples(int samples) {
	mSamples = samples;
}

int LightSourceComponent::getSamples() const {
	return mSamples;
}

void LightSourceComponent::serialize(Json::Value& data) {

}

void LightSourceComponent::deserialize(const Json::Value& data) {

}
