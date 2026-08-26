#pragma once
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class LightingPass : public RenderPass {
	public:
		LightingPass();
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		void updateTextures(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd);

		struct DrawData
		{
			// блок 0..15
			alignas(16) Math::Vec3 viewPos;
			float shadowIntensity = 1.f; // в тот же 16-байтный блок

			// блок 16..31
			float fogStart;
			float fogEnd;
			int   pointLightsSize = 0;
			int   cascadeCount = 0;

			// блок 48..63 (но структура должна быть кратна 16, так что добавим паддинг)
			alignas(16) Math::Vec3 ambientColor;
			float _pad0; // паддинг, чтобы ambientColor занимала 16 байт
		};

		struct CascadData {
			// xy texelSize, z bias, w samples
			Math::Vec4 cascade;
		};
		struct CascadedShadowBlock {
			Math::Vec4 direction_intensity; // xyz direction, w intensity
			Math::Vec4 color_and_misc;
			CascadData data[6];
		};

		CascadedShadowBlock cascadedShadowData;
		DrawData drawData;

		Render::DescriptorSetLayoutHandle mTexSetLayout{};
		Render::DescriptorSetHandle       mTexSet{};
		bool                              mTexSetInitialized = false;

		Render::PipelineHandle            mLightingPipeline{};    // предполагаем, что ты его будешь создавать по RHI
		Render::RenderPassHandle          mLightingRenderPass{};  // если будешь делать отдельный RenderPass

		PipelineHandle pipeline;
		RenderPassHandle renderPass;

		struct PointLight {
			Math::Vec3 Position;
			Math::Vec2 texelSize;
			float bias;
			int samples;
			float radius;
			int offset;

			Math::Vec3 Color;
			float Linear;
			float Quadratic;
			int Type;
			int Layers;
		};

		PointLight pointLight[6];

		BufferHandle pointLightsUBO;
		BufferHandle cascadedShadowsUBO;

		TextureHandle mLitTexture;
		TextureHandle mPresentTex[2];
		FramebufferHandle lightingFBO;
		FramebufferHandle mPresentFbo[2];
		int mPresentShow = 0;
	};
}
