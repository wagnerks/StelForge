#pragma once
#include "assetsModule/modelModule/BoundingVolume.h"
#include "renderModule/FrameBlackboard.h"
#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class PointLightPass : public RenderPassWithData {
	public:
		using Data = Render::PointShadowFrame;

		PointLightPass();
		void prepare() override;
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		void publish(SystemsModule::RenderData& renderDataHandle);
		void collectLights(const FrustumModule::Frustum& camFrustum, const Math::Vec3& cameraPos, std::vector<Math::Mat4>& matrices, std::vector<ecss::SectorId>* entities);
		void ensureGpu(SystemsModule::RenderData& renderDataHandle);

		FramebufferHandle lightFramebuffer;
		TextureHandle mLightDepthMaps;
		SamplerHandle mLightDepthMapsSampler;
		BufferHandle mMatricesUBO;
		RenderPassHandle renderPass;
		PipelineHandle pipeline;
		Render::DescriptorSetLayoutHandle mSetLayout{};
		Render::DescriptorSetHandle mSet{};

		bool mInited = false;
		bool mGpuReady = false;
		Data data{};

		static constexpr int shadowResolution = 1024;
		static constexpr int maxLights = 12;
		static constexpr int facesPerLight = 6;
		static constexpr int maxShadowFaces = maxLights * facesPerLight;
	};
}
