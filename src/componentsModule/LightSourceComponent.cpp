#include "LightSourceComponent.h"

LightSourceComponent::LightSourceComponent(eLightType type) : mType(type) {
}

int LightSourceComponent::getTypeOffset(eLightType type) {
	switch (type) {
	case eLightType::NONE: return 0;
		break;
	case eLightType::DIRECTIONAL:
		return 1;
		break;
	case eLightType::POINT:
		return 6;
		break;
	case eLightType::PERSPECTIVE:
		return 1;
		break;
	case eLightType::WORLD:
		return -1;
		break;
	default: return -1;;
	}

	return -1;
}

Engine::ComponentsModule::eLightType LightSourceComponent::getType() const {
	return mType;
}

void LightSourceComponent::setType(eLightType type) {
	mType = type;
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
