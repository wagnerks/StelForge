#include "BoneAnimationKeys.h"
#include <assimp/anim.h>

namespace AssetsModule {
	BoneAnimationKeys BoneAnimationKeys::readKeys(const aiNodeAnim* channel) {
		BoneAnimationKeys res;

		res.positions.reserve(channel->mNumPositionKeys);
		for (auto i = 0u; i < channel->mNumPositionKeys; ++i) {
			auto& aiPosition = channel->mPositionKeys[i].mValue;
			res.positions.emplace_back(SFE::Math::Vec3{ aiPosition.x, aiPosition.y, aiPosition.z }, static_cast<float>(channel->mPositionKeys[i].mTime));
		}

		res.rotations.reserve(channel->mNumRotationKeys);
		for (auto i = 0u; i < channel->mNumRotationKeys; ++i) {
			auto& aiOrientation = channel->mRotationKeys[i].mValue;
			res.rotations.emplace_back(SFE::Math::Quaternion<float>{ aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z }, static_cast<float>(channel->mRotationKeys[i].mTime));
		}

		res.scales.reserve(channel->mNumScalingKeys);
		for (auto i = 0u; i < channel->mNumScalingKeys; ++i) {
			auto& scale = channel->mScalingKeys[i].mValue;
			res.scales.emplace_back(SFE::Math::Vec3{ scale.x, scale.y, scale.z }, static_cast<float>(channel->mScalingKeys[i].mTime));
		}

		return res;
	}
}

