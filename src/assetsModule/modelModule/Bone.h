#pragma once
#include <vector>
#include <assimp/anim.h>

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

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

    class Bone {
        std::vector<KeyPosition> mPositions;
        std::vector<KeyRotation> mRotations;
        std::vector<KeyScale> mScales;

        SFE::Math::Mat4 mLocalTransform;
        std::string mName;
        int mID;

    public:
        Bone(const std::string& name, int ID, const aiNodeAnim* channel);

        void update(float animationTime);

        const SFE::Math::Mat4& getLocalTransform() const { return mLocalTransform; }
        std::string getBoneName() const { return mName; }
        int getBoneID() const { return mID; }

    private:
        int getPositionIndex(float animationTime) const;
        int getRotationIndex(float animationTime) const;
        int getScaleIndex(float animationTime) const;

        static float calcScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);

        SFE::Math::Mat4 interpolatePosition(float animationTime) const;
        SFE::Math::Mat4 interpolateRotation(float animationTime) const;
        SFE::Math::Mat4 interpolateScaling(float animationTime) const;
    };
}
