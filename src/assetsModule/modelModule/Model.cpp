#include "Model.h"

namespace AssetsModule {
	Model::Model(SFE::Tree<Mesh> model, Armature armature, std::vector<Animation> animations) : mArmature(std::move(armature)), mAnimations(std::move(animations)), mMeshTree(std::move(model)) {
		getLODs();

		if (!mArmature.bones.empty()) {
			mDefaultBoneMatrices.resize(mArmature.bones.size(), mArmature.transform);

			/*std::function<void(Bone*, SFE::Math::Mat4, std::vector<Bone>&)> calculateBoneTransform;
			calculateBoneTransform = [this, &calculateBoneTransform](Bone* node, SFE::Math::Mat4 parentTransform, std::vector<Bone>& bones) {
				parentTransform *= node->transform;
				defaultBoneMatrices[node->id] = parentTransform;

				for (const auto child : node->childrenBones) {
					calculateBoneTransform(&bones[child], parentTransform, bones);
				}
			};

			calculateBoneTransform(&armature.getBones()[0], armature.transform, armature.getBones());*/
		}
	}
	std::vector<Model::LOD>* Model::getLODs() {
		if (mLODs.empty()) {
			for (auto& node : mMeshTree) {
				if (node.value.mData.vertices.empty()) {
					continue;
				}
				if (node.value.lod >= mLODs.size()) {
					mLODs.resize(node.value.lod + 1);
				}

				mLODs[node.value.lod].meshes.push_back(&node.value);
			}
		}

		return &mLODs;
	}

	void Model::recalculateNormals(bool smooth) {
		for (auto& node : mMeshTree) {
			node.value.recalculateNormals(smooth);
		}
	}

	void Model::bindMeshes() {
		for (auto& node : mMeshTree) {
			node.value.initMeshData();
		}

		mLODs.clear();
		getLODs();
	}

	const std::vector<SFE::Math::Mat4>& Model::getDefaultBoneMatrices() {
		return mDefaultBoneMatrices;
	}
}

