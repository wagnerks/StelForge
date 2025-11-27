#pragma once

#include <vector>
#include <chrono> // added for CPU timing

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

		Render::RenderPasses::CascadedShadowPass::Data* mCascadedShadowsPassData = nullptr;
		Render::RenderPasses::PointLightPass::Data* mPointPassData = nullptr;
		Render::RenderPasses::GeometryPass::Data* mGeometryPassData = nullptr;
		Render::RenderPasses::SSAOPass::Data* mSSAOPassData = nullptr;

		RenderMode mRenderType = RenderMode::DEFAULT;

		uint8_t currentRegistry = 0;
		uint8_t nextRegistry = 1;

		void rotate() {
			std::swap(currentRegistry, nextRegistry);
		}
	};

	class RenderDebugInfo : public Singleton<RenderDebugInfo>
	{
	public:
		size_t objectsRendered = 0;
		size_t verticesRendered = 0;
		size_t drawCalls = 0;
	};

	class RenderSystem : public ecss::System {
	public:
		RenderSystem();
		~RenderSystem() override;

		void update(float_t dt) override;
		void debugUpdate(float dt) override;
		inline void setRenderType(RenderMode type) { mRenderData.mRenderType = type; }
		inline RenderData& getRenderData() { return mRenderData; }

		void prepareDataForNextFrame();

		bool isShadowsDebugData() const {
			return mShadowsDebugDataDraw;
		}

		static inline ecss::ECSType types = 0;

		template<typename T>
		static ecss::ECSType getDirtyId() {
			static ecss::ECSType typeId = types++;
			return typeId;
		}

		void notify(Task task) override;

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
		
		bool mShadowsDebugDataDraw = false;
	private:

		void renderFrame();

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
			//auto& entities = dirtiesRanges[getDirtyId<T>()];
			//entities.insert(id);

		}

		//std::unordered_map<ecss::ECSType, ecss::EntitiesRanges> dirtiesRanges;

		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> dirties;
		std::unordered_map<ecss::ECSType, SFE::Vector<std::pair<ecss::EntityId, uint8_t>>> removed;
		std::shared_mutex dirtiesMutex;

		// GPU/CPU profiling data
		unsigned int mGpuTimeQueries[2] {0,0};
		int mCurrentGpuQuery = 0;
		double mLastGpuTimeMs = 0.0; // result from previous frame
		std::chrono::high_resolution_clock::time_point mCpuFrameStart; // per RenderSystem update
		double mLastCpuFrameMs = 0.0; // duration of last RenderSystem::update body
		

		template<typename PassType>
		inline void addRenderPass();

		RenderData mRenderData;
		std::vector<Render::RenderPass*> mRenderPasses;
		std::shared_future<void> updateLock;

		Render::BufferHandle cameraMatricesUBO;

		Render::BufferHandle transforms;
		Render::BufferHandle bones;
	};
}
