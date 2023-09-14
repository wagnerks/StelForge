#pragma once
#include "mat4x4.hpp"
#include <vector>

#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "ecsModule/ComponentBase.h"
#include "renderModule/CascadeShadows.h"

namespace Engine::ComponentsModule {
	struct ShadowCascade {
		Engine::FrustumModule::Frustum frustum = {};
		Engine::ProjectionModule::PerspectiveProjection viewProjection {};
		float bias = 0.f;
		int samples = 64;

		glm::vec2 zMult = { 1.f,1.f };
		glm::vec2 texelSize = {};
	};

	class CascadeShadowComponent : public ecsModule::Component<CascadeShadowComponent>, PropertiesModule::Serializable {
	public:
		CascadeShadowComponent() = default;

		void updateCascades(const ProjectionModule::PerspectiveProjection& cameraProjection);
		void updateLightSpaceMatrices(const glm::mat4& cameraView);

		static glm::mat4 getLightSpaceMatrix(const std::vector<glm::vec4>& corners, const glm::mat4& lightView, float nearMultiplier = 1.f, float farMultiplier = 1.f);
		static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
		static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projView);

		std::vector<ShadowCascade> cascades;
		std::vector<float> shadowCascadeLevels;
		glm::vec2 resolution = {};

		ProjectionModule::PerspectiveProjection mCameraProjection = {};


		void markDirty();
		const std::vector<glm::mat4>& getLightSpaceMatrices();

		void serialize(Json::Value& data) override;
		void deserialize(const Json::Value& data) override;
	private:
		std::vector<glm::mat4> mLightSpaceMatrices;
		bool mDirty = true;
	};
}

using Engine::ComponentsModule::CascadeShadowComponent;