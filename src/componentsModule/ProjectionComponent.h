#pragma once
#include "core/Projection.h"
#include "ecsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class ProjectionComponent : public ecsModule::Component<ProjectionComponent> {
	public:
		ProjectionComponent();
		ProjectionModule::Projection& getProjection();
		void initProjection(glm::vec2 leftBtm, glm::vec2 rightTop, float zNear, float zFar);
		void initProjection(float FOV, float aspect, float zNear, float zFar);
		void initProjection(const ProjectionModule::Projection& projection);
	private:
		ProjectionModule::Projection mProjection;
	};
}

using Engine::ComponentsModule::ProjectionComponent;