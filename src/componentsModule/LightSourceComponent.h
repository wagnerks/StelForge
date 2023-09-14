#pragma once
#include <vec2.hpp>
#include <vec3.hpp>

#include "ecsModule/ComponentBase.h"
#include "propertiesModule/Serializable.h"


namespace Engine::ComponentsModule {
	enum class eLightType {
		NONE = 0,
		DIRECTIONAL,
		POINT,
		PERSPECTIVE
	};

	class ShadowsComponent : public ecsModule::Component<ShadowsComponent> {
		//component just to handle entities which can produce shadows, aka light sources
	};

	class LightSourceComponent : public ecsModule::Component<LightSourceComponent>, public PropertiesModule::Serializable {
	public:
		LightSourceComponent(eLightType type);

		eLightType getType() const;

		void setIntensity(float intensity);
		float getIntensity() const;

		void setLightColor(const glm::vec3& lightColor);
		const glm::vec3& getLightColor() const;

		void setBias(float bias);
		float getBias() const;

		void setTexelSize(const glm::vec2& texelSize);
		const glm::vec2& getTexelSize() const;

		void setSamples(int samples);
		int getSamples() const;

		void serialize(Json::Value& data) override;
		void deserialize(const Json::Value& data) override;

	private:
		float mBias = 0.f;
		glm::vec2 mTexelSize = {};
		int mSamples = 64;

		eLightType mType = eLightType::NONE;

		float mIntensity = 1.f;
		glm::vec3 mLightColor = glm::vec3{ 1.f };

		bool mWithShadows = true;
	};
}

using Engine::ComponentsModule::LightSourceComponent;