#pragma once
#include <vec2.hpp>
#include <vec3.hpp>


#include "componentsModule/ComponentBase.h"
#include "propertiesModule/Serializable.h"


namespace Engine::ComponentsModule {
	enum class eLightType {
		NONE = 0,
		DIRECTIONAL,
		POINT,
		PERSPECTIVE,
		WORLD
	};
	
	class LightSourceComponent : public ecss::ComponentInterface, public PropertiesModule::Serializable {
	public:
		LightSourceComponent(ecss::SectorId id , eLightType type);
		static int getTypeOffset(eLightType type);
		eLightType getType() const;
		void setType(eLightType type);

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
		float mLinear = 0.1f;
		float mQuadratic = 0.0001f;
		float mRadius = 100.f;
		float mNear = 0.1f;
		bool mWithShadows = true;
	private:
		float mBias = 0.004f;
		glm::vec2 mTexelSize = { 0.004f, 0.004f };
		int mSamples = 32;

		eLightType mType = eLightType::NONE;

		float mIntensity = 1.f;
		glm::vec3 mLightColor = glm::vec3{ 1.f };


	};
}

using Engine::ComponentsModule::LightSourceComponent;