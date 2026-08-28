#include "AnimationComputePass.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

#include "assetsModule/modelModule/Animation.h"
#include "assetsModule/modelModule/Armature.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render;
using namespace SFE::Render::RenderPasses;

namespace {
	constexpr int kMaxBones = 100;

	struct GpuBoneDesc {
		SFE::Math::Mat4 offset;
		SFE::Math::Mat4 restLocal;
		int parent = -1;
		int posOff = 0;
		int posCnt = 0;
		int rotOff = 0;
		int rotCnt = 0;
		int sclOff = 0;
		int sclCnt = 0;
		int boneId = 0;
	};
	static_assert(sizeof(GpuBoneDesc) == 160);

	struct GpuVec3Key {
		float x = 0.f, y = 0.f, z = 0.f, time = 0.f;
	};
	static_assert(sizeof(GpuVec3Key) == 16);

	struct GpuQuatKey {
		float x = 0.f, y = 0.f, z = 0.f, w = 1.f, time = 0.f;
		float pad[3]{};
	};
	static_assert(sizeof(GpuQuatKey) == 32);

	struct AnimPush {
		SFE::Math::Mat4 armatureTransform;
		float animTime = 0.f;
		int entityId = 0;
		int boneCount = 0;
		int pad = 0;
	};
	static_assert(sizeof(AnimPush) == 80);

	SFE::Render::BufferHandle createStorage(SFE::Render::RenderHardwareInterface* rhi, size_t size, const void* data, const char* name) {
		SFE::Render::BufferDesc desc;
		desc.size = size < 16 ? 16 : size;
		desc.usage = SFE::Render::BufferUsage::Storage;
		desc.cpuAccess = SFE::Render::CpuAccess::Write;
		desc.debugName = name;
		return rhi->createBuffer(desc, data);
	}

	void collectOrder(const std::vector<AssetsModule::Bone>& bones, uint32_t idx, std::vector<int>& order) {
		if (idx >= bones.size()) {
			return;
		}
		order.push_back(static_cast<int>(idx));
		for (const auto child : bones[idx].childrenBones) {
			collectOrder(bones, child, order);
		}
	}
}

void AnimationComputePass::init(SystemsModule::RenderData&) {
	auto* rhi = Engine::instance()->rhi();

	const auto cs = rhi->createShaderModule({
		ShaderStage::Compute,
		"animate bones cs",
		ShaderModule::ShaderBase::loadShaderCode("shaders/animateBones.cs")
	});
	mPipeline = rhi->createComputePipeline({ cs });

	DescriptorSetLayoutDesc layout;
	layout.bindings = {
		{ 20, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 21, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 22, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 23, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 24, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 25, DescriptorType::StorageBuffer, ShaderStage::Compute },
		{ 2,  DescriptorType::StorageBuffer, ShaderStage::Compute },
	};
	mSetLayout = rhi->createDescriptorSetLayout(layout);
	mSet = rhi->allocateDescriptorSet(mSetLayout);

	BufferDesc instDesc;
	instDesc.size = sizeof(AnimPush) * 4096;
	instDesc.usage = BufferUsage::Storage;
	instDesc.cpuAccess = CpuAccess::Write;
	instDesc.debugName = "AnimInstances";
	mInstancesSSBO = rhi->createBuffer(instDesc);
}

const AnimationComputePass::GpuClip* AnimationComputePass::packClip(const AssetsModule::Animation* animation, const AssetsModule::Armature& armature) {
	if (!animation || armature.bones.empty()) {
		return nullptr;
	}

	if (const auto it = mClips.find(animation); it != mClips.end()) {
		return &it->second;
	}

	std::vector<GpuBoneDesc> descs(armature.bones.size());
	std::vector<GpuVec3Key> posKeys;
	std::vector<GpuQuatKey> rotKeys;
	std::vector<GpuVec3Key> sclKeys;
	std::vector<int> order;
	collectOrder(armature.bones, 0, order);

	for (size_t i = 0; i < armature.bones.size(); ++i) {
		const auto& bone = armature.bones[i];
		auto& desc = descs[i];
		desc.offset = bone.offset;
		desc.restLocal = bone.transform;
		desc.boneId = static_cast<int>(bone.id);
		if (bone.parentBoneIdx == std::numeric_limits<uint32_t>::max() || bone.parentBoneIdx >= armature.bones.size()) {
			desc.parent = -1;
		}
		else {
			desc.parent = static_cast<int>(armature.bones[bone.parentBoneIdx].id);
		}

		const auto* keys = animation->getBoneAnimationInfo(bone.name);
		if (!keys) {
			continue;
		}

		desc.posOff = static_cast<int>(posKeys.size());
		desc.posCnt = static_cast<int>(keys->positions.size());
		for (const auto& k : keys->positions) {
			posKeys.push_back({ k.position.x, k.position.y, k.position.z, k.timeStamp });
		}

		desc.rotOff = static_cast<int>(rotKeys.size());
		desc.rotCnt = static_cast<int>(keys->rotations.size());
		for (const auto& k : keys->rotations) {
			rotKeys.push_back({ k.orientation.x, k.orientation.y, k.orientation.z, k.orientation.w, k.timeStamp, {} });
		}

		desc.sclOff = static_cast<int>(sclKeys.size());
		desc.sclCnt = static_cast<int>(keys->scales.size());
		for (const auto& k : keys->scales) {
			sclKeys.push_back({ k.scale.x, k.scale.y, k.scale.z, k.timeStamp });
		}
	}

	if (posKeys.empty()) {
		posKeys.push_back({});
	}
	if (rotKeys.empty()) {
		rotKeys.push_back({});
	}
	if (sclKeys.empty()) {
		sclKeys.push_back({});
	}
	if (order.empty()) {
		order.push_back(0);
	}

	auto* rhi = Engine::instance()->rhi();
	GpuClip clip;
	clip.boneCount = static_cast<int>(armature.bones.size());
	clip.boneDescs = createStorage(rhi, descs.size() * sizeof(GpuBoneDesc), descs.data(), "AnimBoneDescs");
	clip.posKeys = createStorage(rhi, posKeys.size() * sizeof(GpuVec3Key), posKeys.data(), "AnimPosKeys");
	clip.rotKeys = createStorage(rhi, rotKeys.size() * sizeof(GpuQuatKey), rotKeys.data(), "AnimRotKeys");
	clip.sclKeys = createStorage(rhi, sclKeys.size() * sizeof(GpuVec3Key), sclKeys.data(), "AnimSclKeys");
	clip.order = createStorage(rhi, order.size() * sizeof(int), order.data(), "AnimBoneOrder");

	const auto [it, _] = mClips.emplace(animation, clip);
	return &it->second;
}

void AnimationComputePass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;

	if (!mPipeline || !renderDataHandle.bonesSSBO || !mInstancesSSBO) {
		return;
	}

	auto* rhi = Engine::instance()->rhi();
	const float dt = Engine::instance()->getDeltaTime();

	struct ClipBatch {
		const GpuClip* clip = nullptr;
		std::vector<AnimPush> instances;
	};
	std::unordered_map<const AssetsModule::Animation*, ClipBatch> batches;

	for (auto [entityId, animationComp, armatureComp] : ECSHandler::registry().view<
		ComponentsModule::AnimationComponent,
		ComponentsModule::ArmatureComponent>())
	{
		if (!animationComp || !armatureComp || !animationComp->mCurrentAnimation) {
			continue;
		}

		auto* anim = animationComp->mCurrentAnimation;
		if (animationComp->mPlay || animationComp->step) {
			animationComp->step = false;
			float tps = anim->getTicksPerSecond();
			if (tps <= 0.f) {
				tps = 25.f;
			}
			animationComp->mCurrentTime += tps * dt;
			if (animationComp->mLoop) {
				if (anim->getDuration() > 0.f) {
					animationComp->mCurrentTime = std::fmod(animationComp->mCurrentTime, anim->getDuration());
				}
			}
			else if (anim->getDuration() > 0.f) {
				animationComp->mCurrentTime = std::min(animationComp->mCurrentTime, anim->getDuration());
			}
		}

		if (const auto ocComp = ECSHandler::registry().pinComponent<const ComponentsModule::OcclusionComponent>(entityId);
			ocComp && ocComp->occluded) {
			continue;
		}

		const auto* clip = packClip(anim, armatureComp->armature);
		if (!clip || clip->boneCount <= 0) {
			continue;
		}

		AnimPush inst;
		inst.armatureTransform = armatureComp->armature.transform;
		inst.animTime = animationComp->mCurrentTime;
		inst.entityId = static_cast<int>(entityId);
		inst.boneCount = std::min(clip->boneCount, kMaxBones);

		auto& batch = batches[anim];
		batch.clip = clip;
		batch.instances.push_back(inst);
	}

	if (batches.empty()) {
		return;
	}

	rhi->cmdBindPipeline(cmd, mPipeline);
	bool dispatched = false;
	for (auto& [_, batch] : batches) {
		if (!batch.clip || batch.instances.empty()) {
			continue;
		}

		const auto count = static_cast<uint32_t>(std::min(batch.instances.size(), size_t{ 4096 }));
		rhi->cmdUpdateBuffer(cmd, mInstancesSSBO, 0, count * sizeof(AnimPush), batch.instances.data());
		rhi->updateDescriptorSets({
			{ mSet, 20, DescriptorType::StorageBuffer, {{ batch.clip->boneDescs }} },
			{ mSet, 21, DescriptorType::StorageBuffer, {{ batch.clip->posKeys }} },
			{ mSet, 22, DescriptorType::StorageBuffer, {{ batch.clip->rotKeys }} },
			{ mSet, 23, DescriptorType::StorageBuffer, {{ batch.clip->sclKeys }} },
			{ mSet, 24, DescriptorType::StorageBuffer, {{ batch.clip->order }} },
			{ mSet, 25, DescriptorType::StorageBuffer, {{ mInstancesSSBO }} },
			{ mSet, 2,  DescriptorType::StorageBuffer, {{ renderDataHandle.bonesSSBO }} },
		});
		rhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mSet, 1);
		rhi->cmdDispatch(cmd, count, 1, 1);
		dispatched = true;
	}

	if (!dispatched) {
		return;
	}

	rhi->cmdPipelineBarrier(cmd, {
		PipelineStage::Compute,
		PipelineStage::VertexShader,
		AccessMask::ShaderWrite,
		AccessMask::ShaderRead,
	});
}
