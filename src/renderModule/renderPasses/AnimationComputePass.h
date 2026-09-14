#pragma once

#include <unordered_map>
#include <vector>

#include "rhi/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace AssetsModule {
	class Animation;
	struct Armature;
}

namespace SFE::Render::RenderPasses {

	class AnimationComputePass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

	private:
		struct GpuClip {
			BufferHandle boneDescs;
			BufferHandle posKeys;
			BufferHandle rotKeys;
			BufferHandle sclKeys;
			BufferHandle order;
			// Per clip: cmdUpdateBuffer writes at the call, so two clips dispatched in one list must not
			// share an instance buffer -- every dispatch would read the last clip's instances.
			BufferHandle instances;
			// Bone world matrices without the offset, boneCount per instance: children read their parent's here.
			BufferHandle worlds;
			size_t instanceCapacity = 0;
			int boneCount = 0;
		};

		GpuClip* packClip(const AssetsModule::Animation* animation, const AssetsModule::Armature& armature);

		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mSetLayout{};
		DescriptorSetHandle mSet{};
		std::unordered_map<const AssetsModule::Animation*, GpuClip> mClips;
	};

}
