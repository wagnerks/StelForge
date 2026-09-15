#pragma once

#include "Bone.h"

namespace AssetsModule {
    struct Armature {
        std::string name;
        SFE::Math::Mat4 transform;

        std::vector<Bone> bones;
        // By bone id: how far the farthest vertex weighted to the bone lies from it in its bind space. Posed, a
        // skinned vertex stays within that distance (times the bone's scale) of its bones, which bounds the mesh.
        std::vector<float> boneRadius;
    };
}
