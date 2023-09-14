#pragma once
#include <mutex>
#include <thread>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "componentsModule/CascadeShadowComponent.h"
#include "renderModule/RenderPass.h"


class CascadeShadows;

namespace Engine::RenderModule::RenderPasses {

	class CascadedShadowPass : public RenderPass {
	public:
		struct Data {
			unsigned shadowMapTexture = std::numeric_limits<unsigned>::max();
			glm::vec3 lightDirection = {};
			glm::vec3 lightColor = {};
			glm::vec2 resolution = {};
			float cameraFarPlane = 0.f;
			std::vector<float> shadowCascadeLevels;
			CascadeShadows* shadows = nullptr;
			std::vector<ComponentsModule::ShadowCascade> shadowCascades;
		};

		CascadedShadowPass();
		~CascadedShadowPass() override;
		void init();
		void initRender();
		void freeBuffers() const;

		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		unsigned lightFBO;
		unsigned lightDepthMaps;
		unsigned matricesUBO;

		CascadeShadows* mShadowSource;
		bool mInited = false;
		Data mData;

		std::vector<std::thread> threads;
		std::mutex mtx;

		float mUpdateDelta = 0.03f; //update shadows time, rerender objects with shadow not every frame for optimization purposes
		float mUpdateTimer = std::numeric_limits<float>::max(); //guarantee first frame rendering
	};
}

