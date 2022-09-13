#pragma once
#include <vec2.hpp>
#include <vec3.hpp>

#include "ecsModule/ComponentBase.h"


namespace GameEngine::ComponentsModule {
	enum class eLightType {
		NONE = 0,
		DIRECTIONAL,
		POINT,
		PERSPECTIVE
	};

	class LightComponent : public ecsModule::Component<LightComponent>{
	public:
		LightComponent(eLightType type);

		eLightType getType() const;

		void setBias(float bias);
		float getBias() const;

		void setIntensity(float intensity);
		float getIntensity() const;

		void setTexelSize(const glm::vec2& texelSize);
		const glm::vec2& getTexelSize() const;

		void setSamples(int samples);
		int getSamples() const;

		void setLightColor(const glm::vec3& lightColor);
		const glm::vec3& getLightColor() const;

	private:
		eLightType mType = eLightType::NONE;

		float mIntensity = 1.f;
		float mBias = 0.f;
		glm::vec2 mTexelSize = {};
		int mSamples = 64;
		glm::vec3 mLightColor = glm::vec3{1.f};
	};
}

using GameEngine::ComponentsModule::LightComponent;