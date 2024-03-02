#pragma once
#include <limits>

#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "ecss/Types.h"
#include "glWrapper/Framebuffer.h"
#include "renderModule/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class PointLightPass : public RenderPass {
	public:
		struct Data {
			std::vector<ecss::SectorId> shadowEntities;
		};

		void init() override;
		void freeBuffers() const;

		void render(SystemsModule::RenderData& renderDataHandle) override;
	private:
		GLW::Framebuffer lightFramebuffer;
		GLW::Texture mLightDepthMaps{GLW::TEXTURE_2D_ARRAY};

		GLW::Buffer mMatricesUBO{GLW::UNIFORM_BUFFER };

		void fillMatrix(Math::Vec3 globalLightPos, float lightNear, float lightRadius);
		SFE::ProjectionModule::PerspectiveProjection lightProjection;
		std::vector<Math::Mat4> lightMatrices;
		std::vector<FrustumModule::Frustum> frustums;
		std::vector<std::pair<ecss::SectorId, int>> offsets;

		const int shadowResolution = 1024;
		const int maxShadowFaces = 36;
		const int lightMatricesBinding = 3;

		Data data{};
	};
}
