#pragma once
#include <limits>
#include <vec2.hpp>
#include <vec3.hpp>

#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "renderModule/RenderPass.h"

namespace Engine::RenderModule::RenderPasses {
	class PointLightPass : public RenderPass {
	public:
		struct Data {
			std::vector<ecss::SectorId> shadowEntities;
		};

		void initRender();
		void freeBuffers() const;

		void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) override;
	private:
		unsigned mFramebufferID = 0;
		unsigned mLightDepthMaps = 0;
		unsigned mMatricesUBO = 0;

		void fillMatrix(glm::vec3 globalLightPos, float lightNear, float lightRadius);
		Engine::ProjectionModule::PerspectiveProjection lightProjection;
		std::vector<glm::mat4> lightMatrices;
		std::vector<FrustumModule::Frustum> frustums;
		std::vector<std::pair<ecss::SectorId, int>> offsets;

		const int shadowResolution = 1024;
		const int maxShadowFaces = 36;
		const int lightMatricesBinding = 3;
	};
}
