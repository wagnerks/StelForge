#pragma once
#include <set>
#include <unordered_map>

#include "BoneAnimationKeys.h"


struct aiAnimation;

namespace AssetsModule {
    struct Bone {
        std::string name;
        uint32_t id;

        SFE::Math::Mat4 offset;

        SFE::Math::Mat4 transform {1.f};

        SFE::Math::Vec3 pos;
        SFE::Math::Vec3 scale {1.f};
        SFE::Math::Quaternion<float> rotation;

        uint32_t parentBoneIdx = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> childrenBones;
    };

    struct Armature {
        std::string name;
        SFE::Math::Mat4 transform;

        std::vector<Bone> bones;
    };

    class Animation {
    public:
        Animation() = default;

        Animation(const aiAnimation* animation);
        AssetsModule::BoneAnimationKeys* getBoneAnimationInfo(const std::string& boneName);

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
