#pragma once

#include <vec2.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <mat4x4.hpp>

#include <vector>

#include "core/BoundingVolume.h"
#include "ecsModule/SystemBase.h"
#include "renderModule/RenderPass.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/SSAOPass.h"

namespace GameEngine {
	namespace RenderModule {
		class Renderer;
	}
}

namespace GameEngine::SystemsModule{
	struct RenderDataHandle {
		std::vector<size_t> mDrawableEntities;
		FrustumModule::Frustum camFrustum;

		glm::mat4 projection = {};
		glm::mat4 view = {};
		glm::vec3 cameraPos = {};

		RenderModule::RenderPasses::CascadedShadowPass::Data mCascadedShadowsPassData;
		RenderModule::RenderPasses::GeometryPass::Data mGeometryPassData;
		RenderModule::RenderPasses::SSAOPass::Data mSSAOPassData;
	};

	class RenderSystem : public ecsModule::System<RenderSystem> {
	public:
		RenderSystem(RenderModule::Renderer* renderer);
		void preUpdate(float_t dt) override;
		void update(float_t dt) override;
		void postUpdate(float_t dt) override;
		const std::vector<RenderModule::RenderPass*>& getRenderPasses();
	private:
		RenderModule::Renderer* mRenderer = nullptr;
		std::vector<RenderModule::RenderPass*> mRenderPasses;
		
		RenderDataHandle mRenderData;
	};
}
