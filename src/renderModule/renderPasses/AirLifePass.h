#pragma once

#include <vector>

#include "componentsModule/TerrainComponent.h"
#include "renderModule/renderPasses/FinalDepthTargets.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	// What lives in the air of an outdoor scene: motes of dust and pollen round the camera, lit where the sun reaches
	// them, and swarms of midges over the shallows and flies elsewhere, near the camera. Specks moved in air_life.vs,
	// drawn over the resolved image after TAA; this only picks where the swarms are.
	class AirLifePass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		static constexpr int kSwarms = 8;
		// The swarms nearest the camera, xyz and what dances there as the shader takes them -- midges, flies,
		// butterflies by day or fireflies after sundown, by the height of the sun. False without a terrain.
		bool findSwarms(const Math::Vec3& camera, float sunHeight, float swarms[kSwarms][4]);

		FinalDepthTargets mTargets;
		RenderPassHandle mRenderPass{};
		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mLayout{};
		std::vector<DescriptorSetHandle> mSets; // one per frame slot
		BufferHandle mQuad{};

		bool mTerrainLooked = false;
		ComponentsModule::TerrainComponent mTerrain;
		Math::Vec3 mTerrainPos{};
		// terrainHeights, looked up once: finding them hashes the terrain's whole description.
		const std::vector<float>* mHeights = nullptr;
		float mWaterLevel = 0.f;
	};
}
