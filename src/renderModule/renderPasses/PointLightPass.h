#pragma once

#include "assetsModule/modelModule/BoundingVolume.h"
#include "mathModule/Projection.h"
#include "ecss/Types.h"
#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class PointLightPass : public RenderPass {
	public:
		struct Data {
			std::vector<ecss::SectorId> shadowEntities;
		};

		void init(SystemsModule::RenderData& renderDataHandle) override;
		void freeBuffers() const;

		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:
		FramebufferHandle lightFramebuffer;
	
		TextureHandle mLightDepthMaps;
		SamplerHandle mLightDepthMapsSampler;

		BufferHandle mMatricesUBO;

		RenderPassHandle renderPass;
		PipelineHandle pipeline;

		Render::DescriptorSetLayoutHandle mTexSetLayout{};
		Render::DescriptorSetHandle       mTexSet{};

		void fillMatrix(Math::Vec3 globalLightPos, float lightNear, float lightRadius);
		SFE::MathModule::PerspectiveProjection lightProjection;
		std::vector<Math::Mat4> lightMatrices;
		std::vector<FrustumModule::Frustum> frustums;
		std::vector<std::pair<ecss::SectorId, int>> offsets;

		const int shadowResolution = 1024;
		const int maxShadowFaces = 36;

		Data data{};
	};
}
