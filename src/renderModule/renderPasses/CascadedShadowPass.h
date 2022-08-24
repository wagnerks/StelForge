#pragma once
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "renderModule/RenderPass.h"

class CascadeShadows;

namespace GameEngine::RenderModule::RenderPasses {
	class CascadedShadowPass : public RenderPass {
	public:
		struct Data {
			float bias = 0.f;
			unsigned shadowMapTexture = std::numeric_limits<unsigned>::max();
			glm::vec3 lightDirection = {};
			glm::vec3 lightColor = {};
			glm::vec2 resolution = {};
			glm::vec3 pos = {};
			float cameraFarPlane = 0.f;
			float texelSize = 0.5f;
			std::vector<float> shadowCascadeLevels;
			CascadeShadows* shadows;
			int samples = 5;
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

