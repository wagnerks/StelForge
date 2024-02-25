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

		Model(SFE::Tree<Mesh> model, Armature armature, std::vector<Animation> animations);

		void bindMeshes();
		void recalculateNormals(bool smooth = true);

	public:
		struct LOD {
			std::vector<Mesh*> meshes;
		};

		std::vector<LOD>* getLODs();
		const std::vector<Animation>& getAnimations() { return mAnimations; }
		const std::vector<SFE::Math::Mat4>& getDefaultBoneMatrices();
		const Armature& getArmature() { return mArmature; }

	private:
		Armature mArmature;

		std::vector<SFE::Math::Mat4> mDefaultBoneMatrices;
		std::vector<Animation> mAnimations;
		std::vector<LOD> mLODs;

		SFE::Tree<Mesh> mMeshTree;
	};
}
