#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "componentsModule/CascadeShadowComponent.h"
#include "ecss/EntityHandle.h"
#include "renderModule/Buffer.h"
#include "renderModule/Framebuffer.h"

#include "renderModule/RenderPass.h"


namespace SFE::Render::RenderPasses {

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

		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		void updateRenderData(SystemsModule::RenderData& renderDataHandle) const;
		void debug(SystemsModule::RenderData& renderDataHandle);

		Render::Framebuffer lightFBO;
		AssetsModule::Texture lightDepthMap{AssetsModule::TEXTURE_2D_ARRAY};

		Buffer matricesUBO{ UNIFORM_BUFFER };

		ecss::EntityHandle mShadowSource;
		bool mInited = false;
		Data mData;
	};
}

