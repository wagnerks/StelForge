#pragma once
#include <limits>

#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "ecss/Types.h"
#include "renderModule/RenderPass.h"

namespace SFE::RenderModule::RenderPasses {
	class PointLightPass : public RenderPass {
	public:
		struct Data {
			std::vector<ecss::SectorId> shadowEntities;
		};

		void init() override;
		void freeBuffers() const;

		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;
	private:
		unsigned mFramebufferID = 0;
		unsigned mLightDepthMaps = 0;
		unsigned mMatricesUBO = 0;

		void fillMatrix(Math::Vec3 globalLightPos, float lightNear, float lightRadius);
		SFE::ProjectionModule::PerspectiveProjection lightProjection;
		std::vector<Math::Mat4> lightMatrices;
		std::vector<FrustumModule::Frustum> frustums;
		std::vector<std::pair<ecss::SectorId, int>> offsets;

		const int shadowResolution = 1024;
		const int maxShadowFaces = 36;
		const int lightMatricesBinding = 3;
	};
}
