#pragma once

#include "Bone.h"

namespace AssetsModule {
    struct Armature {
        std::string name;
        SFE::Math::Mat4 transform;

        std::vector<Bone> bones;

        // Every skinned mesh gets a box for the current pose, grown from the posed joints of the bones that move it.
        // Past kMaxPoseBounds meshes the last box covers the rest.
        static constexpr uint32_t kMaxPoseBounds = 32;
        uint32_t poseBoundsCount = 0;
        // By bone id: how far the farthest vertex weighted to the bone lies from it in its bind space (a posed vertex
        // stays within that distance, times the bone's scale, of one of its bones), and a bit per pose box it moves.
        std::vector<float> boneRadius;
        std::vector<uint32_t> boneBoundsMask;
    };
}
