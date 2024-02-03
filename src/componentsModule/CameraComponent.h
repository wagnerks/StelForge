#pragma once
#include "core/Projection.h"
#include "componentsModule/ComponentBase.h"
#include "core/BoundingVolume.h"

namespace SFE::ComponentsModule {
	class CameraComponent : public ecss::ComponentInterface {
	public:
		CameraComponent(ecss::SectorId id);
		CameraComponent(ecss::SectorId id, float FOV, float aspect, float zNear, float zFar);
		ProjectionModule::PerspectiveProjection& getProjection();

		void initProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection(const ProjectionModule::PerspectiveProjection& projection);
		void updateFrustum(const Math::Mat4& view) const;
		const FrustumModule::Frustum& getFrustum() const;
	private:
		ProjectionModule::PerspectiveProjection mProjection;
		mutable Math::Mat4 mViewCash;
		mutable FrustumModule::Frustum mFrustum;
		Math::Vec3 extents{};
	};
}

using SFE::ComponentsModule::CameraComponent;