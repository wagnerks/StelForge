#pragma once
#include "mathModule/Projection.h"
#include "componentsModule/ComponentBase.h"
#include "assetsModule/modelModule/BoundingVolume.h"

namespace SFE::ComponentsModule {
	class CameraComponent : public ecss::ComponentInterface {
	public:
		CameraComponent(ecss::SectorId id);
		CameraComponent(ecss::SectorId id, float FOV, float aspect, float zNear, float zFar);
		MathModule::PerspectiveProjection& getProjection();

		void initProjection(const float FOV, float aspect, float zNear, float zFar);
		void initProjection(const MathModule::PerspectiveProjection& projection);
		void updateFrustum(const Math::Mat4& view) const;
		const FrustumModule::Frustum& getFrustum() const;
	private:
		MathModule::PerspectiveProjection mProjection;
		mutable Math::Mat4 mViewCash;
		mutable FrustumModule::Frustum mFrustum;
		Math::Vec3 extents{};
	};
}

using SFE::ComponentsModule::CameraComponent;