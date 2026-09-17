#pragma once

#include <unordered_map>
#include <vector>

#include "ecss/Types.h"
#include "mathModule/Forward.h"
#include "mathModule/Matrix.h"
#include "rhi/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace AssetsModule {
	class Animation;
	struct Armature;
}

namespace SFE::Render::RenderPasses {

	// Evaluates every animated character's pose in one dispatch. Each character has a record on the GPU that
	// changes only when its animation or armature does; the time comes from Engine::animationClock() in the shader.
	class AnimationComputePass : public RenderPass {
	public:
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void prepare(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;

		struct GpuBoneDesc {
			Math::Mat4 offset;
			Math::Mat4 restLocal;
			int parent = -1; // within the clip
			int posOff = 0;
			int posCnt = 0;
			int rotOff = 0;
			int rotCnt = 0;
			int sclOff = 0;
			int sclCnt = 0;
			int boneId = 0;
			float radius = 0.f;      // Armature::boneRadius
			uint32_t boundsMask = 0; // Armature::boneBoundsMask
			// Ticks between a channel's keys where every gap is the same, so the shader works out which key it is
			// in instead of searching for it; 0 where the gaps differ.
			float posStep = 0.f;
			float rotStep = 0.f;
			float sclStep = 0.f;
			float pad[3]{};
		};

		struct GpuVec3Key {
			float x = 0.f, y = 0.f, z = 0.f, time = 0.f;
		};

		struct GpuQuatKey {
			float x = 0.f, y = 0.f, z = 0.f, w = 1.f, time = 0.f;
			float pad[3]{};
		};

		struct GpuRecord {
			Math::Mat4 armatureTransform;
			uint32_t boneOffset = 0; // the entity's range in the bone buffer, also its rows in the worlds buffer
			uint32_t descOffset = 0; // the clip's first bone desc and bone order entry
			uint32_t boneCount = 0;
			uint32_t flags = 0; // playing, loop, and the pose bounds count in bits 8-15
			float start = 0.f; // seconds, relative to mEpoch
			float ticksPerSecond = 0.f;
			float duration = 0.f;
			float stoppedTime = 0.f;
			// The clip faded out of, laid out over the same armature; flags 0 when there is no fade.
			uint32_t fromDescOffset = 0;
			uint32_t fromFlags = 0; // kPlaying, kLoop, kBlending
			float fromStart = 0.f;
			float fromTicksPerSecond = 0.f;
			float fromDuration = 0.f;
			float fromStoppedTime = 0.f;
			float blendStart = 0.f; // seconds, relative to mEpoch
			float blendDuration = 0.f;
		};

	private:
		struct ClipInfo {
			uint32_t descOffset = 0;
			uint32_t boneCount = 0;
		};

		const ClipInfo* packClip(const AssetsModule::Animation* animation, const AssetsModule::Armature& armature);
		void writeRecord(ecss::EntityId entity);
		void removeRecord(ecss::EntityId entity);
		void markRecord(uint32_t index);
		void upload();

		PipelineHandle mPipeline{};
		DescriptorSetLayoutHandle mSetLayout{};
		DescriptorSetHandle mSet{};

		// Every clip's data, one after another.
		std::unordered_map<const AssetsModule::Animation*, ClipInfo> mClips;
		std::vector<GpuBoneDesc> mDescs;
		std::vector<GpuVec3Key> mPosKeys;
		std::vector<GpuQuatKey> mRotKeys;
		std::vector<GpuVec3Key> mSclKeys;
		std::vector<int> mOrder; // bone indices within the clip, parents first
		bool mClipsDirty = false;
		BufferHandle mDescBuffer;
		BufferHandle mPosBuffer;
		BufferHandle mRotBuffer;
		BufferHandle mSclBuffer;
		BufferHandle mOrderBuffer;

		// Dense: a removed record takes the last one's place.
		std::vector<GpuRecord> mRecords;
		std::vector<ecss::EntityId> mRecordEntities;
		std::unordered_map<ecss::EntityId, uint32_t> mRecordIndex;
		BufferHandle mRecordBuffer;
		size_t mRecordCapacity = 0;
		uint32_t mDirtyLo = UINT32_MAX;
		uint32_t mDirtyHi = 0;

		// Bone world matrices without the offset, indexed like the bone buffer: children read their parent's here.
		BufferHandle mWorlds;
		size_t mWorldsRows = 0;
		size_t mWorldsCapacity = 0;

		// The clock value the records' start times are relative to. Moved forward now and then, so the float
		// time the shader gets stays small enough to keep its precision.
		double mEpoch = 0.0;
	};

}
