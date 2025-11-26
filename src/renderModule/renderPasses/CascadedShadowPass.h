#pragma once
#include <mutex>
#include <thread>
#include <vector>

#include "componentsModule/CascadeShadowComponent.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/Framebuffer.h"
#include "renderModule/RenderHardwareInterface.h"

#include "renderModule/renderPasses/RenderPass.h"


namespace SFE::Render::RenderPasses {

	class CascadedShadowPass : public RenderPassWithData {
	public:
		void prepare() override;


		struct Data {
			TextureHandle shadowMapTexture;
			SamplerHandle shadowMapSampler;
			Math::Vec3 lightDirection = {};
			Math::Vec3 lightColor = {};
			Math::Vec2 resolution = {};
			float cameraFarPlane = 0.f;
			std::vector<float> shadowCascadeLevels;
			ecss::EntityId shadows;
			std::vector<ComponentsModule::ShadowCascade> shadowCascades;
			float shadowsIntensity = 0.f;
		};

		CascadedShadowPass();
		~CascadedShadowPass() override;
		void init() override;
		void initRender();

		void render(SystemsModule::RenderData& renderDataHandle) override;
	private:
		void updateRenderData(SystemsModule::RenderData& renderDataHandle);
		void debug(SystemsModule::RenderData& renderDataHandle);

		std::vector<FramebufferHandle> lightFbos;
		TextureHandle  lightDepthMa;
		SamplerHandle  lightSampler;

		BufferHandle matricesUBO;
		PipelineHandle pipeline;
		RenderPassHandle renderPass;

		ecss::EntityId mShadowSource;
		bool mInited = false;
		Data mData;
	};
}

