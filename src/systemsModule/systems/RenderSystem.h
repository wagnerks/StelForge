#pragma once

#include <vector>

#include "assetsModule/modelModule/BoundingVolume.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/OutlineComponent.h"
#include "componentsModule/TransformComponent.h"
#include "systemsModule/SystemBase.h"
#include "renderModule/renderPasses/RenderPass.h"
#include "renderModule/renderPasses/CascadedShadowPass.h"
#include "renderModule/renderPasses/GeometryPass.h"
#include "renderModule/renderPasses/PointLightPass.h"
#include "renderModule/renderPasses/SSAOPass.h"

namespace SFE {
	namespace Render {
		class Renderer;
	}
}

namespace SFE::SystemsModule {

	enum class RenderMode {
		DEFAULT,
		WIREFRAME,
		NORMALS,
	};

	struct RenderMatrices {
		Math::Mat4 projection = {};
		Math::Mat4 view = {};
		Math::Mat4 PV = {};
	};

	struct RenderData {
		FrustumModule::Frustum mCamFrustum;
		FrustumModule::Frustum mNextCamFrustum;

		RenderMatrices current;
		RenderMatrices next;

		Math::Vec3 mCameraPos = {};
		Math::Vec3 mViewDir = {};
		Math::Vec3 mNextCameraPos = {};
		Math::Vec3 mNextViewDir = {};

		MathModule::PerspectiveProjection cameraProjection = {};
		MathModule::PerspectiveProjection nextCameraProjection =  {};

		Render::RenderPasses::CascadedShadowPass::Data* mCascadedShadowsPassData;
		Render::RenderPasses::PointLightPass::Data* mPointPassData;
		Render::RenderPasses::GeometryPass::Data* mGeometryPassData;
		Render::RenderPasses::SSAOPass::Data* mSSAOPassData;

		RenderMode mRenderType = RenderMode::DEFAULT;

		uint8_t currentRegistry = 0;
		uint8_t nextRegistry = 1;

		void rotate() {
			std::swap(currentRegistry, nextRegistry);
		}
	};

	class RenderSystem : public ecss::System {
	public:
		RenderSystem();
		~RenderSystem() override;

		void update(float_t dt) override;
		void debugUpdate(float dt) override;
		inline void setRenderType(RenderMode type) { mRenderData.mRenderType = type; }
		inline RenderData& getRenderData() { return mRenderData; }

		bool isShadowsDebugData() const {
			return mShadowsDebugDataDraw;
		}

		static inline ecss::ECSType types = 0;

		template<typename T>
		static ecss::ECSType getDirtyId() {
			static ecss::ECSType typeId = types++;
			return typeId;
		}

		template<typename CompType>
		void markDirty(ecss::EntityId id) {
			if constexpr
			(
				std::is_same_v<CompType, MaterialComponent> ||
				std::is_same_v<CompType, ComponentsModule::TransformMatComp> ||
				std::is_same_v<CompType, ComponentsModule::ArmatureBonesComponent> ||
				std::is_same_v<CompType, OutlineComponent> ||
				std::is_same_v<CompType, MeshComponent> ||
				std::is_same_v<CompType, ComponentsModule::OccludedComponent>
			)
			{
				markDirtyImpl<CompType>(id);
			}
		}

		template<typename T>
		void markRemoved(ecss::EntityId id) {
			auto lock = std::unique_lock(dirtiesMutex);
			auto& entities = removed[getDirtyId<T>()];

			if (auto it = entities.find([id](const std::pair<ecss::EntityId, uint8_t>& a) { return a.first == id; })) {
				it->second = 2;
			}
			else {
				entities.emplace_back(id, 2);
			}
		}

		
	private:

		template<typename T>
		void markDirtyImpl(ecss::EntityId id) {
			auto lock = std::unique_lock(dirtiesMutex);
			auto& entities = dirties[getDirtyId<T>()];

			if (auto it = entities.find([id](const std::pair<ecss::EntityId, uint8_t>& a) { return a.first == id; })) {
				it->second = 2;
			}
			else {
				entities.emplace_back(id, 2);
			}
		}

		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> dirties;
		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> removed;
		std::shared_mutex dirtiesMutex;

		bool mGeometryPassDataWindow = false;
		bool mShadowsDebugDataDraw = false;

		template<typename PassType>
		inline void addRenderPass();

		RenderData mRenderData;
		std::vector<Render::RenderPass*> mRenderPasses;
		std::shared_future<void> updateLock;
		GLW::Buffer cameraMatricesUBO{GLW::UNIFORM_BUFFER};
	};
}
