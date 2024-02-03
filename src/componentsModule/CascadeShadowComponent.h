#pragma once

#include <vector>

#include "ComponentBase.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "propertiesModule/Serializable.h"


namespace SFE::ComponentsModule {
	struct ShadowCascade {
		SFE::FrustumModule::Frustum frustum = {};
		SFE::ProjectionModule::PerspectiveProjection viewProjection {};
		float bias = 0.f;
		int samples = 64;

		Math::Vec2 zMult = { 1.f,1.f };
		Math::Vec2 texelSize = {};
	};

	class CascadeShadowComponent : public ecss::ComponentInterface, PropertiesModule::Serializable {
	public:
		CascadeShadowComponent(ecss::SectorId id) : ComponentInterface(id) {};

		void updateCascades(const ProjectionModule::PerspectiveProjection& cameraProjection);
		void updateLightSpaceMatrices(const Math::Mat4& cameraView);

		static SFE::Math::Mat4 getLightSpaceMatrix(const std::vector<SFE::Math::Vec4>& corners, const SFE::Math::Mat4& lightView, float nearMultiplier = 1.f, float farMultiplier = 1.f);
		static std::vector<Math::Vec4> getFrustumCornersWorldSpace(const Math::Mat4& proj, const Math::Mat4& view);
		static std::vector<Math::Vec4> getFrustumCornersWorldSpace(const Math::Mat4& projView);

		std::vector<ShadowCascade> cascades;
		std::vector<float> shadowCascadeLevels;
		Math::Vec2 resolution = {};

		ProjectionModule::PerspectiveProjection mCameraProjection = {};


		void markDirty();
		void calculateLightSpaceMatrices(const ProjectionModule::PerspectiveProjection& projection, const Math::Mat4& view);
		const std::vector<Math::Mat4>& getLightSpaceMatrices();

		void serialize(Json::Value& data) override;
		void deserialize(const Json::Value& data) override;
		float shadowIntensity = 1.f;

		void cacheMatrices();

		static const std::vector<Math::Mat4>& getCacheLightSpaceMatrices();
		static void debugDraw(const std::vector<SFE::Math::Mat4>& lightSpaceMatrices, const Math::Mat4& cameraProjection, const Math::Mat4& cameraView);
		static void clearCacheMatrices();
	private:
		static void drawCascadeVolumeVisualizers(const std::vector<SFE::Math::Mat4>& lightMatrices, SFE::ShaderModule::ShaderBase* shader);
		static inline std::vector<Math::Mat4> mLightMatricesCache;


		std::vector<Math::Mat4> mLightSpaceMatrices;
		bool mDirty = true;
	};
}

using SFE::ComponentsModule::CascadeShadowComponent;