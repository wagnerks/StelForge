#pragma once
#include "mat4x4.hpp"
#include <vector>

#include "ecsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class CascadeShadowsHolderComponent : public ecsModule::Component<CascadeShadowsHolderComponent> {
	public:
		CascadeShadowsHolderComponent() = default;
		static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
		static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projView);
	private:
	};

	class CascadeShadowComponent : public ecsModule::Component<CascadeShadowComponent> {
	public:
		CascadeShadowComponent() = default;
	//private:
		// Tune this parameter according to the scene
		float mZMult = 2000.0f;

		float mBiasMultiplier = 0.0000025f;
		float mTexelsMultiplier = 0.1f;
		glm::mat4 projectionFromCamera = {};
	};
}

using Engine::ComponentsModule::CascadeShadowsHolderComponent;
using Engine::ComponentsModule::CascadeShadowComponent;