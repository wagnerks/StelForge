#pragma once
#include <memory>
#include <vector>

#include "Animation.h"
#include "Mesh.h"
#include "assetsModule/Asset.h"
#include "mathModule/Quaternion.h"
#include "nodeModule/TreeNode.h"


namespace AssetsModule {
	struct ModelObj {
		std::vector<MeshHandle> mMeshHandles;
	};

	struct MeshNode : SFE::NodeModule::TreeNode<MeshNode> {
		MeshNode() = default;

		MeshNode(MeshNode&& other) noexcept
			: TreeNode<MeshNode>(std::move(other)),
			mLods(std::move(other.mLods))
		{}

		MeshNode& operator=(MeshNode&& other) noexcept {
			TreeNode<MeshNode>::operator =(std::move(other));
			if (this == &other) {
				return *this;
			}

			mLods = std::move(other.mLods);
			return *this;
		}

		std::vector<std::vector<Mesh>> mLods;
	};

	class Model : public Asset {
	public:

		Model(MeshNode model, std::string_view modelPath, Armature armature);

		Armature arma;

		std::vector<ModelObj>* getAllLODs();

		ModelObj toModelObj(int lod);

		std::string_view getModelPath();

		void normalizeModel(bool smooth = true);

		void bindMeshes();

		bool normalized = false;
		int mLODs = 0;

		std::vector<Animation> animations;

		std::vector<SFE::Math::Mat4> defaultBoneMatrices;

		void calculateLODs();
		void toModelObjHelper(MeshNode* root, int lod, ModelObj& res);
		std::vector<ModelObj> lods;
		MeshNode mMeshTree;
		std::string mModelPath = "";
	};
}
