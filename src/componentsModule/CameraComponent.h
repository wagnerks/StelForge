#pragma once
#include "core/Projection.h"
#include "ecss/base/ComponentBase.h"

namespace Engine::ComponentsModule {
	class CameraComponent : public ecss::Component<CameraComponent> {
	public:
		CameraComponent();
		CameraComponent(float FOV, float aspect, float zNear, float zFar);
		ProjectionModule::PerspectiveProjection& getProjection();

		void initProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection(const ProjectionModule::PerspectiveProjection& projection);
	private:
		ProjectionModule::PerspectiveProjection mProjection;
	};
}

using Engine::ComponentsModule::CameraComponent;