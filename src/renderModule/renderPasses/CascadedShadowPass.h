#pragma once
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "renderModule/RenderPass.h"

class CascadeShadow;
class CascadeShadows;

namespace GameEngine::RenderModule::RenderPasses {
	class CascadedShadowPass : public RenderPass {
	public:
		struct Data {
			unsigned shadowMapTexture = std::numeric_limits<unsigned>::max();
			glm::vec3 lightDirection = {};
			glm::vec3 lightColor = {};
			glm::vec2 resolution = {};
			glm::vec3 pos = {};
			float cameraFarPlane = 0.f;
			std::vector<float> shadowCascadeLevels;
			CascadeShadows* shadows = nullptr;
			std::vector<CascadeShadow*> shadowCascades;
		};

		CascadedShadowPass();
		void init();
		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		CascadeShadows* mShadowSource;
		bool mInited = false;
		Data mData;
	};
}

