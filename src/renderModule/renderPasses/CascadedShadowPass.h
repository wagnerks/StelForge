#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "componentsModule/CascadeShadowComponent.h"
#include "ecss/EntityHandle.h"

#include "renderModule/RenderPass.h"


namespace SFE::RenderModule::RenderPasses {

	class CascadedShadowPass : public RenderPassWithData {
	public:
		void prepare() override;


		struct Data {
			unsigned shadowMapTexture = std::numeric_limits<unsigned>::max();
			Math::Vec3 lightDirection = {};
			Math::Vec3 lightColor = {};
			Math::Vec2 resolution = {};
			float cameraFarPlane = 0.f;
			std::vector<float> shadowCascadeLevels;
			ecss::EntityHandle shadows;
			std::vector<ComponentsModule::ShadowCascade> shadowCascades;
			float shadowsIntensity = 0.f;
		};

		CascadedShadowPass();
		~CascadedShadowPass() override;
		void init() override;
		void initRender();
		void freeBuffers() const;

		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		void updateRenderData(SystemsModule::RenderData& renderDataHandle) const;
		void debug(SystemsModule::RenderData& renderDataHandle);

		unsigned lightFBO;
		unsigned lightDepthMaps;
		unsigned matricesUBO;

		ecss::EntityHandle mShadowSource;
		bool mInited = false;
		Data mData;

		float mUpdateDelta = 0.03f; //update shadows time, rerender objects with shadow not every frame for optimization purposes
		float mUpdateTimer = std::numeric_limits<float>::max(); //guarantee first frame rendering
	};
}

