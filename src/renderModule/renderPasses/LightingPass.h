#pragma once
#include "renderModule/renderPasses/RenderPass.h"
#include "renderModule/IblBaker.h"

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
			float viewPosX = 0.f, viewPosY = 0.f, viewPosZ = 0.f;
			float shadowIntensity = 1.f;
			float fogStart = 0.f;
			float fogEnd = 0.f;
			int   pointLightsSize = 0;
			int   cascadeCount = 0;
			float ambientX = 0.f, ambientY = 0.f, ambientZ = 0.f;
			float ssaoAmount = 1.f;
			float viewDirX = 0.f, viewDirY = 0.f, viewDirZ = 1.f;
			float iblStrength = 1.f;
			float shadowAmbient = 0.42f;
			float padLook0 = 0.f; // SSS wrap
			float padLook1 = 0.f; // SSS strength
			// Backend-parity debugging: non-zero makes deferred_shading.fs output one isolated
			// term instead of the lit result, so each term can be diffed on its own. Reuses the
			// last free pad slot, so the UBO layout does not change.
			float debugTerm = 0.f;
		};

		struct CascadData {
			// x 1/resolution, y world size of one texel, z depth bias, w samples
			Math::Vec4 cascade;
			// x light-space depth extent of the cascade, so the shader can turn a depth difference
			// into a world distance; the rest is spare. std140 gives the array a 32 byte stride
			// with two vec4s, which is what this struct is.
			Math::Vec4 cascadeExtra;
		};
		struct CascadedShadowBlock {
			Math::Vec4 direction_intensity; // xyz direction, w intensity
			Math::Vec4 color_and_misc;
			CascadData data[6];
			Math::Vec4 cascadeSplits;
			Math::Vec4 cascadeSplits2;
		};

		CascadedShadowBlock cascadedShadowData;
		DrawData drawData;

		Render::DescriptorSetLayoutHandle mTexSetLayout{};
		Render::DescriptorSetHandle       mTexSet{};
		Render::DescriptorSetLayoutHandle mUboSetLayout{};
		Render::DescriptorSetHandle       mUboSet{};
		bool                              mTexSetInitialized = false;

		Render::PipelineHandle            mLightingPipeline{};    // предполагаем, что ты его будешь создавать по RHI
		Render::RenderPassHandle          mLightingRenderPass{};  // если будешь делать отдельный RenderPass

		PipelineHandle pipeline;
		RenderPassHandle renderPass;

		struct PointLight {
			Math::Vec3 Position;
			float pad0 = 0.f;
			Math::Vec2 texelSize;
			float bias = 0.f;
			int samples = 0;
			float radius = 0.f;
			int offset = 0;
			float pad1 = 0.f;
			float pad2 = 0.f;
			Math::Vec3 Color;
			float Linear = 0.f;
			float Quadratic = 0.f;
			int Type = 0;
			int Layers = 0;
			float pad3 = 0.f;
		};
		static_assert(sizeof(PointLight) == 80);

		PointLight pointLight[12];

		BufferHandle pointLightsUBO;
		BufferHandle cascadedShadowsUBO;
		BufferHandle pushConstantsUBO;
		BufferHandle lightMatricesUBO;
		TextureHandle mDummyPointShadow;
		SamplerHandle mDummyPointShadowSampler;

		TextureHandle mPresentTex[2];
		SamplerHandle mPresentSampler{};
		FramebufferHandle mPresentFbo[2];
		int mPresentShow = 0;

		IblMaps mIbl;

		struct BoundImage {
			TextureHandle texture;
			SamplerHandle sampler;
		};
		BoundImage mBoundGPos{};
		BoundImage mBoundGNormal{};
		BoundImage mBoundGAlbedo{};
		BoundImage mBoundSsao{};
		BoundImage mBoundPointShadow{};
		BoundImage mBoundOutlines{};
		BoundImage mBoundCsm{};
		BoundImage mBoundCsmDepth{};
		BoundImage mBoundIblIrr{};
		BoundImage mBoundIblPref{};
		BoundImage mBoundIblLut{};
		BufferHandle mBoundLightMat{};
		BufferHandle mBoundPointMat{};
		BufferHandle mBoundPc{};
		BufferHandle mBoundCam{};
	};
}
