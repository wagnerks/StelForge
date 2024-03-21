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
		void recalculateNormals(bool smooth = true);

	public:
		struct LOD {
			std::vector<SFE::MeshObject3D*> meshes;
		};

		std::vector<LOD>* getLODs();
		const std::vector<Animation>& getAnimations() const { return mAnimations; }
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
