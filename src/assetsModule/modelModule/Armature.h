#pragma once

#include "Bone.h"

namespace AssetsModule {
    struct Armature {
        std::string name;
        SFE::Math::Mat4 transform;

        std::vector<Bone> bones;
    };
}
