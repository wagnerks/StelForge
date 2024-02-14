#include "Animation.h"

AssetsModule::Animation::Animation(const std::string& animationPath, Model* model) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
	assert(scene && scene->mRootNode);
	if (!scene->mAnimations) {
		return;
	}
	auto animation = scene->mAnimations[0];

          
	std::set<std::string> bones;
	bones.insert("Armature");
            

	mDuration = animation->mDuration;
	mTicksPerSecond = animation->mTicksPerSecond;
	readHierarchyData(mRootNode, scene->mRootNode);
	readMissingBones(animation, *model);

	if (scene->mNumMeshes) {
		for (auto s = 0; s < scene->mMeshes[0]->mNumBones; s++) {
			bones.insert(scene->mMeshes[0]->mBones[s]->mName.C_Str());
		}
	}
	std::function<bool(AssimpNodeData&)> hasChild;
	hasChild = [&bones, &hasChild](AssimpNodeData& root) {
		if (bones.contains(root.name)) {
			return true;
		}
		for (auto child : root.children) {
			if (bones.contains(child.name)) {
				return true;
			}
			if (hasChild(child)) {
				return true;
			}
		}

		return false;
	};

	std::function<void(AssimpNodeData&)> foreach;
	foreach = [&hasChild, &foreach](AssimpNodeData& node) {
		for (auto it = node.children.begin(); it != node.children.end();) {
			if (hasChild(*it)) {
				foreach(*it);
				++it;
			}
			else {
				it = node.children.erase(it);
			}
		}
	           
	};

	foreach(mRootNode);
}

AssetsModule::Bone* AssetsModule::Animation::findBone(const std::string& name) {
	auto iter = std::find_if(mBones.begin(), mBones.end(),
	[&name](const Bone& Bone)  
		{
			return Bone.getBoneName() == name;
		}
	);

	if (iter == mBones.end()) {
		return nullptr;
	}

	return &(*iter);
}

void AssetsModule::Animator::calculateBoneTransform(const AssimpNodeData* node, SFE::Math::Mat4 parentTransform) {
	std::string nodeName = node->name;
	SFE::Math::Mat4 nodeTransform = node->transformation;

	if (auto bone = mCurrentAnimation->findBone(nodeName)) {
		bone->update(mCurrentTime);
		nodeTransform = bone->getLocalTransform();
	}

	SFE::Math::Mat4 globalTransformation = parentTransform * nodeTransform;

	auto boneInfoMap = mCurrentAnimation->getBoneIDMap();
	if (boneInfoMap.contains(nodeName)) {
		auto offset = boneInfoMap[nodeName].offset;
		mFinalBoneMatrices[boneInfoMap[nodeName].id] = globalTransformation * offset;
	}

	for (int i = 0; i < node->children.size(); i++) {
		calculateBoneTransform(&node->children[i], globalTransformation);
	}
}
