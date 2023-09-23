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
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/SSAOPass.h"

namespace Engine {
	namespace RenderModule {
		class Renderer;
	}
}

namespace Engine::SystemsModule {

	enum class RenderMode {
		DEFAULT,
		WIREFRAME,
		NORMALS,
	};

	struct RenderDataHandle {
		std::vector<size_t> mDrawableEntities;
		FrustumModule::Frustum mCamFrustum;

		glm::mat4 mProjection = {};
		glm::mat4 mView = {};
		glm::vec3 mCameraPos = {};

		RenderModule::RenderPasses::CascadedShadowPass::Data mCascadedShadowsPassData;
		RenderModule::RenderPasses::PointLightPass::Data mPointPassData;
		RenderModule::RenderPasses::GeometryPass::Data mGeometryPassData;
		RenderModule::RenderPasses::SSAOPass::Data mSSAOPassData;

		RenderMode mRenderType = RenderMode::DEFAULT;
	};

	class RenderSystem : public ecsModule::System<RenderSystem> {
	public:
		void setRenderType(RenderMode type);
		RenderSystem(RenderModule::Renderer* renderer);
		void preUpdate(float_t dt) override;
		void update(float_t dt) override;
		void postUpdate(float_t dt) override;
		const std::vector<RenderModule::RenderPass*>& getRenderPasses();
		~RenderSystem() override;
	private:
		RenderModule::Renderer* mRenderer = nullptr;
		std::vector<RenderModule::RenderPass*> mRenderPasses;

		RenderDataHandle mRenderData;

		bool mGeometryPassDataWindow = false;
		bool mShadowsDebugDataDraw = false;
	};
}
