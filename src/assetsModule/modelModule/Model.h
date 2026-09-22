#pragma once
#include <memory>
#include <vector>

#include "Animation.h"
#include "Armature.h"
#include "Mesh.h"
#include "assetsModule/Asset.h"
#include "containersModule/Tree.h"


namespace AssetsModule {
	class Model : public Asset {
	public:
		Model(const Model& other) = delete;
		Model(Model&& other) noexcept = delete;
		Model& operator=(const Model& other) = delete;
		Model& operator=(Model&& other) noexcept = delete;

		Model(SFE::Tree<SFE::MeshObject3D> model, Armature armature, std::vector<Animation> animations);

		void bindMeshes();
		// Armature::poseBoundsCount, boneRadius and boneBoundsMask, and Mesh::poseBounds of every skinned mesh.
		void measurePoseBounds();
		void recalculateNormals(bool smooth = true);

	public:
		struct LOD {
			std::vector<SFE::MeshObject3D*> meshes;
		};

		std::vector<LOD>* getLODs();
		const std::vector<Animation>& getAnimations() const { return mAnimations; }
		// The clips here were made on their own skeleton; the one they are played on is `played`. Where the two put a
		// bone in different places, the clip was plainly not made for that bone, and everything it says about it --
		// place, turn and size alike -- is thrown away, leaving the bone where the model rests it. A bone the two
		// agree on is untouched, so a clip that carries the body about keeps doing it. Comes back with how many bones
		// were freed.
		size_t keepOwnBones(const Armature& played);
		const std::vector<SFE::Math::Mat4>& getDefaultBoneMatrices() const ;
		const Armature& getArmature() const { return mArmature; }
		const SFE::Tree<SFE::MeshObject3D>& getMeshTree() const { return mMeshTree; }
	private:
		Armature mArmature;

		std::vector<SFE::Math::Mat4> mDefaultBoneMatrices;
		std::vector<Animation> mAnimations;
		std::vector<LOD> mLODs;
		
		SFE::Tree<SFE::MeshObject3D> mMeshTree;
	};
}
