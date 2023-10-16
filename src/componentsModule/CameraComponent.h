#pragma once
#include "core/Projection.h"
#include "componentsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class CameraComponent : public ecss::ComponentInterface {
	public:
		CameraComponent(ecss::EntityId id);
		CameraComponent(ecss::EntityId id, float FOV, float aspect, float zNear, float zFar);
		ProjectionModule::PerspectiveProjection& getProjection();

		void initProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection(const ProjectionModule::PerspectiveProjection& projection);
	private:
		ProjectionModule::PerspectiveProjection mProjection;
	};
}

using Engine::ComponentsModule::CameraComponent;