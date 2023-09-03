#include "LodComponent.h"

#include <algorithm>

using namespace Engine::ComponentsModule;

LodComponent::LodComponent(eLodType type): mLodType(type) {
}

void LodComponent::setLodLevel(size_t lodLevel) {
	mLodLevel = lodLevel;
}

size_t LodComponent::getLodLevel() const {
	return mLodLevel;
}

eLodType LodComponent::getLodType() const {
	return mLodType;
}

void LodComponent::setLodType(eLodType type) {
	mLodType = type;
}

const std::vector<float>& LodComponent::getLodLevelValues() {
	return mLodLevelValues;
}

void LodComponent::addLodLevelValue(float lodLevel) {
	mLodLevelValues.emplace_back(lodLevel);
	std::ranges::sort(mLodLevelValues);
	std::ranges::reverse(mLodLevelValues);
}

float LodComponent::getCurrentLodValue() const {
	return mCurrentLodValue;
}

void LodComponent::setCurrentLodValue(float currentLodValue) {
	mCurrentLodValue = currentLodValue;
}
