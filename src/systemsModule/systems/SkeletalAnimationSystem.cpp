#include "SkeletalAnimationSystem.h"

#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "core/ECSHandler.h"

namespace SFE::SystemsModule {
	void SkeletalAnimationSystem::update(float dt) {
		for (auto [entityId, animationComp, armatureComp, ocComp] : ECSHandler::registry().forEach<ComponentsModule::AnimationComponent, ComponentsModule::ArmatureComponent, ComponentsModule::OcclusionComponent>()) {
			if (animationComp->mCurrentAnimation && (animationComp->mPlay || animationComp->step)) {
				animationComp->step = false;
				animationComp->mCurrentTime += animationComp->mCurrentAnimation->getTicksPerSecond() * dt;
				animationComp->mCurrentTime = fmod(animationComp->mCurrentTime, animationComp->mCurrentAnimation->getDuration());
				if (ocComp && ocComp->occluded) {
					continue;
				}
				updateAnimation(animationComp->mCurrentAnimation, animationComp->mCurrentTime, armatureComp->armature, &armatureComp->boneMatrices[0]);
			}
		}
	}

	template <typename KeyType>
	size_t SkeletalAnimationSystem::getKeyIndex(float animationTime, const std::vector<KeyType>& keys) {
		assert(keys.size() >= 2);
		size_t index = 1;
		for (; index < keys.size(); index++) {
			if (animationTime < keys[index].timeStamp) {
				return index - 1;
			}
		}
		return keys.size() - 2;
	}

	void SkeletalAnimationSystem::updateAnimation(const AssetsModule::Animation* animation, float& currentTime, AssetsModule::Armature& armature, Math::Mat4* boneMatrices) {
		if (!animation) {
			return;
		}

		if (!armature.bones.empty()) {
			calculateBoneTransform(animation, currentTime, &armature.bones[0], armature.transform, armature.bones, boneMatrices);
		}
	}

	void SkeletalAnimationSystem::calculateBoneTransform(const AssetsModule::Animation* animation, float currentTime, AssetsModule::Bone* bone, SFE::Math::Mat4 parentTransform,	std::vector<AssetsModule::Bone>& bones, Math::Mat4* boneMatrices) {
		if (const auto animationKeys = animation->getBoneAnimationInfo(bone->name)) {
			bone->pos = interpolatePosition(currentTime, animationKeys->positions);
			bone->rotation = interpolateRotation(currentTime, animationKeys->rotations);
			bone->scale = interpolateScaling(currentTime, animationKeys->scales);

			bone->transform = translate(SFE::Math::Mat4(1.f), bone->pos) * bone->rotation.toMat4() * SFE::Math::scale(SFE::Math::Mat4(1.f), bone->scale);
		}

		parentTransform *= bone->transform;

		boneMatrices[bone->id] = parentTransform * bone->offset;

		for (const auto child : bone->childrenBones) {
			calculateBoneTransform(animation, currentTime, &bones[child], parentTransform, bones, boneMatrices);
		}
	}

	float SkeletalAnimationSystem::calcScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) {
		const float midWayLength = animationTime - lastTimeStamp;
		const float framesDiff = nextTimeStamp - lastTimeStamp;
		return midWayLength / framesDiff;
	}

	Math::Vec3 SkeletalAnimationSystem::interpolatePosition(float curTime, const std::vector<AssetsModule::KeyPosition>& positionKeys) {
		if (positionKeys.size() == 1) {
			return SFE::Math::Mat4(1.0f), positionKeys[0].position;
		}

		const auto curIndex = getKeyIndex(curTime, positionKeys);

		const auto& current = positionKeys[curIndex];
		const auto& next = positionKeys[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, curTime);
		return mix(current.position, next.position, scaleFactor);
	}

	Math::Quaternion<float> SkeletalAnimationSystem::interpolateRotation(float curTime, const std::vector<AssetsModule::KeyRotation>& rotationKeys) {
		if (rotationKeys.size() == 1) {
			return normalize(rotationKeys[0].orientation);
		}

		const auto curIndex = getKeyIndex(curTime, rotationKeys);

		const auto& current = rotationKeys[curIndex];
		const auto& next = rotationKeys[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, curTime);
		return normalize(slerp(current.orientation, next.orientation, scaleFactor));
	}

	Math::Vec3 SkeletalAnimationSystem::interpolateScaling(float curTime, const std::vector<AssetsModule::KeyScale>& scaleKeys) {
		if (scaleKeys.size() == 1) {
			return scaleKeys[0].scale;
		}

		const auto curIndex = getKeyIndex(curTime, scaleKeys);

		const auto& current = scaleKeys[curIndex];
		const auto& next = scaleKeys[curIndex + 1];

		const auto scaleFactor = calcScaleFactor(current.timeStamp, next.timeStamp, curTime);
		return mix(current.scale, next.scale, scaleFactor);
	}
}

