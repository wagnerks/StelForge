#pragma once
#include "assetsModule/modelModule/Animation.h"
#include "assetsModule/modelModule/Armature.h"
#include "assetsModule/modelModule/BoneAnimationKeys.h"
#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class SkeletalAnimationSystem : public ecss::System {
	public:
		void update(float dt) override;
	private:
        void updateAnimation(const AssetsModule::Animation* animation, float& currentTime, AssetsModule::Armature& armature, std::vector<Math::Mat4>& boneMatrices);

        void calculateBoneTransform(const AssetsModule::Animation* animation, float currentTime, AssetsModule::Bone* bone, Math::Mat4 parentTransform, std::vector<AssetsModule::Bone>& bones, std::vector<Math::Mat4>& boneMatrices);

		static float calcScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);

        static Math::Quaternion<float> interpolateRotation(float curTime, const std::vector<AssetsModule::KeyRotation>& rotationKeys);
        static Math::Vec3 interpolatePosition(float curTime, const std::vector<AssetsModule::KeyPosition>& positionKeys);
        static Math::Vec3 interpolateScaling(float curTime, const std::vector<AssetsModule::KeyScale>& scaleKeys);

		template<typename KeyType>
		static size_t getKeyIndex(float animationTime, const std::vector<KeyType>& keys);
	};
}
