#pragma once
#include <set>
#include <unordered_map>

#include "BoneAnimationKeys.h"


struct aiAnimation;

namespace AssetsModule {
    class Animation {
    public:
        Animation() = default;

        Animation(const aiAnimation* animation);
        const AssetsModule::BoneAnimationKeys* getBoneAnimationInfo(const std::string& boneName) const;

        inline float getTicksPerSecond() const { return mTicksPerSecond; }
        inline float getDuration() const { return mDuration; }

   // private:
        static void readKeys(const aiAnimation* animation, std::unordered_map<std::string, BoneAnimationKeys>& keys);
        std::string mName;
        float mDuration;
        float mTicksPerSecond;
        std::unordered_map<std::string, BoneAnimationKeys> mBoneAnimationInfos;
    };
}
