#pragma once

#include <unordered_map>
#include <vector>

#include "renderModule/RenderHardwareInterface.h"
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
			int boneCount = 0;
		};

		const GpuClip* packClip(const AssetsModule::Animation* animation, const AssetsModule::Armature& armature);

		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mSetLayout{};
		DescriptorSetHandle mSet{};
		BufferHandle mInstancesSSBO{};
		std::unordered_map<const AssetsModule::Animation*, GpuClip> mClips;
	};

}
