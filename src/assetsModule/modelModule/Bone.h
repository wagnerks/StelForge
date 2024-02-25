#pragma once
#include <vector>

#include "mathModule/Forward.h"

namespace AssetsModule {
    struct Bone {
        std::string name;
        uint32_t id;

        SFE::Math::Mat4 offset{1.f};

        SFE::Math::Mat4 transform {1.f};

        SFE::Math::Vec3 pos;
        SFE::Math::Vec3 scale {1.f};
        SFE::Math::Quat rotation;

        uint32_t parentBoneIdx = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> childrenBones;
    };
}
