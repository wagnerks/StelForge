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
#include "renderModule/FrameBlackboard.h"

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

	struct GraphicsSettings {
		bool ssao = true;
		bool ssr = true;
		bool volumetric = true;
		bool taa = true;
		bool dof = true;
		bool bloom = true;

		float ssrDistance = 12.f;

		float volumetricDensity = 0.022f;
		float volumetricIntensity = 2.4f;
		float volumetricAnisotropy = 0.68f;
		float volumetricDistance = 70.f;

		float taaBlend = 0.92f;

		float dofFocus = 18.f;
		float dofRange = 28.f;
		float dofBlur = 3.5f;

		float bloomStrength = 0.62f;
		float exposure = 0.92f;
		float vignette = 0.14f;
		float chromaticAberration = 0.0028f;

		float fogStart = 0.45f;
		float glassRoughness = 0.02f;
	};

	struct RenderData {
		FrustumModule::Frustum mCamFrustum;
		FrustumModule::Frustum mNextCamFrustum;

		RenderMatrices current;
		RenderMatrices next;
		Math::Mat4 prevPV = {};

		Math::Vec3 mCameraPos = {};
		Math::Vec3 mViewDir = {};
		Math::Vec3 mNextCameraPos = {};
		Math::Vec3 mNextViewDir = {};

		MathModule::PerspectiveProjection cameraProjection = {};
		MathModule::PerspectiveProjection nextCameraProjection =  {};

		Render::FrameBlackboard frame;

		Render::BufferHandle transformsSSBO;
		Render::BufferHandle bonesSSBO;
		Render::BufferHandle cameraMatricesUBO;

		RenderMode mRenderType = RenderMode::DEFAULT;
		GraphicsSettings graphics;

		uint8_t currentRegistry = 0;
		uint8_t nextRegistry = 1;
		int taaFrame = 0;

		void rotate() {
			std::swap(currentRegistry, nextRegistry);
		}
	};

		class RenderDebugInfo : public Singleton<RenderDebugInfo>
	{
	public:
		struct PassTime {
			const char* name = "";
			float cpuMs = 0.f;
		};

		size_t objectsRendered = 0;
		size_t verticesRendered = 0;
		size_t drawCalls = 0;
		size_t cullConsidered = 0;
		size_t cullVisible = 0;
		size_t cullShadow = 0;
		size_t shadowMeshes = 0;
		size_t shadowFbos = 0;
		size_t shadowFbosTotal = 0;
		uint32_t shadowPipeline = 0;
		float shadowIntensity = 0.f;

		PassTime passTimes[16]{};
		int passCount = 0;
		float cpuMs = 0.f;
		float submitMs = 0.f;
		float presentMs = 0.f;
		float gpuMs = 0.f;
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
		bool shouldRunPass(size_t passIndex) const;
		void ensureGpuCapacity(ecss::EntityId id);

		template<typename T>
		void markDirtyImpl(ecss::EntityId id) {
			auto lock = std::unique_lock(dirtiesMutex);
			dirties[getDirtyId<T>()].emplace_back(id, 2);
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

		RenderData mRenderData;
		std::vector<Render::RenderPass*> mRenderPasses;
		std::vector<const char*> mPassNames;
		std::shared_future<void> updateLock;

		Render::BufferHandle cameraMatricesUBO;

		Render::BufferHandle transforms;
		Render::BufferHandle bones;
		size_t mGpuCapacity = 0;

		Render::DescriptorSetLayoutHandle frameSetLayout;
		Render::DescriptorSetHandle frameSet;
	};
}
