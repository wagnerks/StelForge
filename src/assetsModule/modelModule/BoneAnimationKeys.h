#pragma once
#include <vector>


#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

struct aiNodeAnim;

namespace AssetsModule {
    struct KeyPosition {
        SFE::Math::Vec3 position;
        float timeStamp;
    };

    struct KeyRotation {
        SFE::Math::Quaternion<float> orientation;
        float timeStamp;
    };

    struct KeyScale {
        SFE::Math::Vec3 scale;
        float timeStamp;
    };

    struct BoneAnimationKeys {
        static BoneAnimationKeys readKeys(const aiNodeAnim* channel);
        BoneAnimationKeys() = default;

        std::vector<KeyPosition> positions;
        std::vector<KeyRotation> rotations;
        std::vector<KeyScale> scales;
    };
}
