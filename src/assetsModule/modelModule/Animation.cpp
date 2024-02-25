#include "Animation.h"
#include <assimp/anim.h>

#include <map>

AssetsModule::Animation::Animation(const aiAnimation* animation) {
	mDuration = static_cast<float>(animation->mDuration);
	mTicksPerSecond = static_cast<float>(animation->mTicksPerSecond);
	mName = animation->mName.C_Str();

	readKeys(animation, mBoneAnimationInfos);
}

const AssetsModule::BoneAnimationKeys* AssetsModule::Animation::getBoneAnimationInfo(const std::string& boneName) const {
	const auto it = mBoneAnimationInfos.find(boneName);
	if (it == mBoneAnimationInfos.end()) {
		return nullptr;
	}

	return &(it->second);
}

void AssetsModule::Animation::readKeys(const aiAnimation* animation, std::unordered_map<std::string, BoneAnimationKeys>& keys) {
	for (auto i = 0u; i < animation->mNumChannels; i++) {
		const auto channel = animation->mChannels[i];

		std::string boneName = channel->mNodeName.C_Str();
		keys.emplace(boneName, BoneAnimationKeys::readKeys(channel));
	}
}