#pragma once
#include <set>
#include <vector>
#include <unordered_map>

#include "BoneAnimationKeys.h"


struct aiAnimation;

namespace AssetsModule {
    class Animation {
    public:
        Animation() = default;

        Animation(const aiAnimation* animation);
        // A clip `weight` of the way from `a` to `b`, key by key: for two clips made alike (the same keys at the same
        // times), as a pose made for a small thing and one for a big one. A bone keyed differently in them keeps a's.
        static Animation between(const Animation& a, const Animation& b, float weight);
        const AssetsModule::BoneAnimationKeys* getBoneAnimationInfo(const std::string& boneName) const;
        // Forget everything this clip says about these bones, so they stay where the model rests them. For bones the
        // clip was plainly not made for: a hand refitted to new geometry, a library of clips shared between
        // characters. Their turns go with their places -- a turn belongs to the frame the bone rested in, and on a
        // bone that has since moved it is as wrong as the place. Comes back with how many bones were freed.
        size_t forgetBones(const std::vector<std::string>& bones);

        inline float getTicksPerSecond() const { return mTicksPerSecond; }
        inline float getDuration() const { return mDuration; }
        inline const std::string& getName() const { return mName; }

    private:
        static void readKeys(const aiAnimation* animation, std::unordered_map<std::string, BoneAnimationKeys>& keys);
        std::string mName;
        float mDuration;
        float mTicksPerSecond;
        std::unordered_map<std::string, BoneAnimationKeys> mBoneAnimationInfos;
    };
}
