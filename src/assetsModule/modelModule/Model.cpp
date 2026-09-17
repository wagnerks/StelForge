#include "Model.h"
#include "MeshLod.h"

#include "MeshUtils.h"
#include "MeshVaoRegistry.h"

namespace AssetsModule {
	Model::Model(SFE::Tree<SFE::MeshObject3D> model, Armature armature, std::vector<Animation> animations) : mArmature(std::move(armature)), mAnimations(std::move(animations)), mMeshTree(std::move(model)) {
		if (!mArmature.bones.empty()) {
			measurePoseBounds();
		}
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

	void Model::measurePoseBounds() {
		std::vector<const SFE::Math::Mat4*> offsets;
		for (const auto& bone : mArmature.bones) {
			if (bone.id >= offsets.size()) {
				offsets.resize(bone.id + 1, nullptr);
			}
			offsets[bone.id] = &bone.offset;
		}
		mArmature.boneRadius.assign(offsets.size(), 0.f);
		mArmature.boneBoundsMask.assign(offsets.size(), 0u);
		mArmature.poseBoundsCount = 0;
		for (auto& node : mMeshTree) {
			auto& mesh = node.value.mesh;
			mesh.poseBounds = 0;
			const uint32_t slot = (std::min)(mArmature.poseBoundsCount, Armature::kMaxPoseBounds - 1);
			bool skinned = false;
			for (const auto& v : mesh.vertices) {
				for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
					const int id = v.boneIDs[i];
					if (id < 0 || static_cast<size_t>(id) >= offsets.size() || !offsets[id] || v.weights[i] <= 0.f) {
						continue;
					}
					skinned = true;
					const SFE::Math::Vec4 local = *offsets[id] * SFE::Math::Vec4(v.position, 1.f);
					mArmature.boneRadius[id] = (std::max)(mArmature.boneRadius[id], SFE::Math::length(SFE::Math::Vec3(local)));
					mArmature.boneBoundsMask[id] |= 1u << slot;
				}
			}
			if (skinned) {
				mesh.poseBounds = slot + 1;
				mArmature.poseBoundsCount = (std::min)(mArmature.poseBoundsCount + 1, Armature::kMaxPoseBounds);
			}
		}
	}

	void Model::bindMeshes() {
		std::vector<SFE::Mesh<SFE::Vertex3D>*> meshes;
		for (auto& node : mMeshTree) {
			// Before the upload: the registry puts the whole chain in the index buffer in one go. A mesh that comes with
			// its levels already (the terrain, from its cache) keeps them, and the vertices they index.
			if (node.value.mesh.lodIndices.empty()) {
				SFE::weldMesh(node.value.mesh);
				SFE::buildMeshLods(node.value.mesh);
			}
			meshes.push_back(&node.value.mesh);
		}
		SFE::MeshVaoRegistry::instance()->initMeshes(meshes);

		mLODs.clear();
		getLODs();
	}

	const std::vector<SFE::Math::Mat4>& Model::getDefaultBoneMatrices() const {
		return mDefaultBoneMatrices;
	}
}

