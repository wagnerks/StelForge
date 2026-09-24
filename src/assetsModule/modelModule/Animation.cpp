#include "Animation.h"
#include <assimp/anim.h>

#include <cmath>
#include <map>

AssetsModule::Animation::Animation(const aiAnimation* animation) {
	mDuration = static_cast<float>(animation->mDuration);
	mTicksPerSecond = static_cast<float>(animation->mTicksPerSecond);
	mName = animation->mName.C_Str();

	readKeys(animation, mBoneAnimationInfos);
}

AssetsModule::Animation::Animation(std::string name, const float duration, const float ticksPerSecond)
    : mName(std::move(name)), mDuration(duration), mTicksPerSecond(ticksPerSecond) {}

size_t AssetsModule::Animation::forgetBones(const std::vector<std::string>& bones) {
    size_t freed = 0;
    for (const auto& bone : bones) {
        const auto found = mBoneAnimationInfos.find(bone);
        if (found == mBoneAnimationInfos.end()) {
            continue;
        }
        auto& keys = found->second;
        if (keys.positions.empty() && keys.rotations.empty() && keys.scales.empty()) {
            continue;
        }
        keys.positions.clear();
        keys.rotations.clear();
        keys.scales.clear();
        ++freed;
    }
    return freed;
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
AssetsModule::Animation AssetsModule::Animation::between(const Animation& a, const Animation& b, float weight) {
	Animation mixed = a;
	for (auto& [bone, keys] : mixed.mBoneAnimationInfos) {
		const auto* other = b.getBoneAnimationInfo(bone);
		if (!other) {
			continue;
		}
		if (other->positions.size() == keys.positions.size()) {
			for (size_t i = 0; i < keys.positions.size(); ++i) {
				keys.positions[i].position = keys.positions[i].position + (other->positions[i].position - keys.positions[i].position) * weight;
			}
		}
		if (other->scales.size() == keys.scales.size()) {
			for (size_t i = 0; i < keys.scales.size(); ++i) {
				keys.scales[i].scale = keys.scales[i].scale + (other->scales[i].scale - keys.scales[i].scale) * weight;
			}
		}
		if (other->rotations.size() == keys.rotations.size()) {
			for (size_t i = 0; i < keys.rotations.size(); ++i) {
				auto& q = keys.rotations[i].orientation;
				auto r = other->rotations[i].orientation;
				if (q.w * r.w + q.x * r.x + q.y * r.y + q.z * r.z < 0.f) { // the short way
					r = { -r.w, -r.x, -r.y, -r.z };
				}
				const float w = q.w + (r.w - q.w) * weight, x = q.x + (r.x - q.x) * weight;
				const float y = q.y + (r.y - q.y) * weight, z = q.z + (r.z - q.z) * weight;
				const float length = std::sqrt(w * w + x * x + y * y + z * z);
				q = { w / length, x / length, y / length, z / length };
			}
		}
	}
	return mixed;
}
