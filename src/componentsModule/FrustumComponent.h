#pragma once
#include "core/BoundingVolume.h"
#include "ecsModule/ComponentBase.h"

namespace GameEngine::ComponentsModule {
	class FrustumComponent : public ecsModule::Component<FrustumComponent> {
	public:
		FrustumComponent();
		void updateFrustum(const ProjectionModule::Projection& projection, const glm::mat4& view);
		void updateFrustum(const glm::mat4& projView);
		void updateFrustum(const glm::mat4& projection, const glm::mat4& view);
		bool isOnFrustum(const FrustumModule::BoundingVolume& bVolume) const;
		FrustumModule::Frustum* getFrustum();
	private:
		FrustumModule::Frustum mFrustum;
	};
}

using GameEngine::ComponentsModule::FrustumComponent;