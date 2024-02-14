#pragma once
#include <set>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Bone.h"
#include "Model.h"
namespace AssetsModule {
    struct AssimpNodeData {
	    SFE::Math::Mat4 transformation;
        std::string name;
        std::vector<AssimpNodeData> children;
    };

    

    class Animation {
    public:
        Animation() = default;

        Animation(const std::string& animationPath, Model* model);

        ~Animation() {}

        Bone* findBone(const std::string& name);


        inline float getTicksPerSecond() const { return mTicksPerSecond; }

        inline float getDuration() const { return mDuration; }

        inline const AssimpNodeData& getRootNode() { return mRootNode; }

        inline const std::map<std::string, BoneInfo>& getBoneIDMap() {
            return mBoneInfoMap;
        }

    private:
        void readMissingBones(const aiAnimation* animation, Model& model) {
            auto boneInfoMap = model.getBoneInfoMap();
            auto boneCount = boneInfoMap.size();

            //reading channels(bones engaged in an animation and their keyframes)
            for (auto i = 0u; i < animation->mNumChannels; i++) {
                auto channel = animation->mChannels[i];
                std::string boneName = channel->mNodeName.data;

                if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
                    boneInfoMap[boneName].id = boneCount;
                    boneCount++;
                }
                mBones.push_back(Bone(boneName, boneInfoMap[channel->mNodeName.data].id, channel));
            }

            mBoneInfoMap = boneInfoMap;
        }

        void readHierarchyData(AssimpNodeData& dest, const aiNode* src) {
            assert(src);
         
            dest.name = src->mName.data;
            auto& from = src->mTransformation;
            dest.transformation = { {from.a1, from.a2, from.a3, from.a4}, {from.b1, from.b2, from.b3, from.b4}, {from.c1, from.c2, from.c3, from.c4}, {from.d1, from.d2, from.d3, from.d4} };
            auto& to = dest.transformation;
            to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
            to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
            to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
            to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
            for (auto i = 0u; i < src->mNumChildren; i++) {
                AssimpNodeData newData;
                readHierarchyData(newData, src->mChildren[i]);
                dest.children.push_back(newData);
            }
        }

        float mDuration;
        int mTicksPerSecond;
        std::vector<Bone> mBones;
        AssimpNodeData mRootNode;
        std::map<std::string, BoneInfo> mBoneInfoMap;

        Armature arma;
    };


    class Animator {
    public:
        Animator() {
            mFinalBoneMatrices.reserve(100);

            for (int i = 0; i < 100; i++) {
                mFinalBoneMatrices.push_back(SFE::Math::Mat4(1.0f));
            }
        }

        Animator(Animation* animation) {
            mCurrentAnimation = animation;

            mFinalBoneMatrices.reserve(100);

            for (int i = 0; i < 100; i++) {
                mFinalBoneMatrices.push_back(SFE::Math::Mat4(1.0f));
            }
        }

        void updateAnimation(float dt) {
            mDeltaTime = dt;
            if (mCurrentAnimation) {
                mCurrentTime += mCurrentAnimation->getTicksPerSecond() * dt;
                mCurrentTime = fmod(mCurrentTime, mCurrentAnimation->getDuration());
                calculateBoneTransform(&mCurrentAnimation->getRootNode(), SFE::Math::Mat4(1.0f));
            }
        }

        void playAnimation(Animation* pAnimation) {
            mCurrentAnimation = pAnimation;
            mCurrentTime = 0.0f;
        }

        void calculateBoneTransform(const AssimpNodeData* node, SFE::Math::Mat4 parentTransform);

        std::vector<SFE::Math::Mat4> getFinalBoneMatrices() {
            return mFinalBoneMatrices;
        }

    private:

        std::vector<SFE::Math::Mat4> mFinalBoneMatrices;
        Animation* mCurrentAnimation = nullptr;;
        float mCurrentTime = 0.f;
        float mDeltaTime = 0.f;
    };
}