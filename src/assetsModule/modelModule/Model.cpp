#include "Model.h"

#include "MeshUtils.h"
#include "MeshVaoRegistry.h"

namespace AssetsModule {
	Model::Model(SFE::Tree<SFE::MeshObject3D> model, Armature armature, std::vector<Animation> animations) : mArmature(std::move(armature)), mAnimations(std::move(animations)), mMeshTree(std::move(model)) {
		recalculateNormals(true);
		bindMeshes();

		if (!mArmature.bones.empty()) {
			mDefaultBoneMatrices.resize(mArmature.bones.size(), mArmature.transform);

			//std::function<void(Bone*, SFE::Math::Mat4, std::vector<Bone>&)> calculateBoneTransform;
			//calculateBoneTransform = [this, &calculateBoneTransform](Bone* node, SFE::Math::Mat4 parentTransform, std::vector<Bone>& bones) {
			//	parentTransform *= node->transform;
			//	mDefaultBoneMatrices[node->id] = parentTransform;
			//	
			//	for (const auto child : node->childrenBones) {
			//		calculateBoneTransform(&bones[child], parentTransform, bones);
			//	}
			//};
			//
			//calculateBoneTransform(&mArmature.bones[0], {}/*mArmature.transform*/, mArmature.bones);
		}
	}

	std::vector<Model::LOD>* Model::getLODs() {
		if (mLODs.empty()) {
			for (auto& node : mMeshTree) {
				if (node.value.mesh.vertices.empty()) {
					continue;
				}
				if (0/*node.value.mesh.lod*/ >= mLODs.size()) {
					mLODs.resize(/*node.value.mesh.lod +*/ 1);
				}

				mLODs[/*node.value.mesh.lod*/0].meshes.push_back(&node.value);
			}
		}

		return &mLODs;
	}

	void Model::recalculateNormals(bool smooth) {
		for (auto& node : mMeshTree) {
			SFE::MeshUtils::recalculateNormals(&node.value.mesh, smooth);
		}
	}

	void Model::bindMeshes() {
		for (auto& node : mMeshTree) {
			SFE::MeshVaoRegistry::instance()->initMesh(&node.value.mesh);
		}

		mLODs.clear();
		getLODs();
	}

	const std::vector<SFE::Math::Mat4>& Model::getDefaultBoneMatrices() const {
		return mDefaultBoneMatrices;
	}
}

