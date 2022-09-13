#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"

namespace GameEngine::ComponentsModule {
	enum eLodType {
		NONE = 0,
		DISTANCE,
		SCREEN_SPACE
	};

	class LodComponent : public ecsModule::Component<LodComponent> {
	public:
		LodComponent(eLodType type = eLodType::NONE);
		void setLodLevel(size_t lodLevel);
		size_t getLodLevel() const;

		eLodType getLodType() const;
		void setLodType(eLodType type);
		const std::vector<float>& getLodLevelValues();
		void addLodLevelValue(float lodLevel);

		float getCurrentLodValue() const;
		void setCurrentLodValue(float currentLodValue);
	private:
		size_t mLodLevel = 0;
		eLodType mLodType;
		float mCurrentLodValue = 0.f;

		std::vector<float> mLodLevelValues;

	};
}

using GameEngine::ComponentsModule::LodComponent;