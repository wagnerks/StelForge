#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"
#include "assetsModule/Asset.h"
#include "nodeModule/TreeNode.h"


namespace AssetsModule {
	struct ModelObj {
		std::vector<MeshHandle> mMeshHandles;
	};

	struct MeshNode : Engine::NodeModule::TreeNode<MeshNode> {
		MeshNode() = default;

		MeshNode(MeshNode&& other) noexcept
			: TreeNode<MeshNode>(std::move(other)),
			mMeshes(std::move(other.mMeshes))
		{}

		MeshNode& operator=(MeshNode&& other) noexcept {
			TreeNode<MeshNode>::operator =(std::move(other));
			if (this == &other) {
				return *this;
			}

			mMeshes = std::move(other.mMeshes);
			return *this;
		}

		std::vector<std::vector<Mesh>> mMeshes;
	};

	class Model : public Asset {
	public:
		Model(MeshNode& model, std::string_view modelPath);

		std::vector<ModelObj> getAllLODs();

		ModelObj toModelObj(int lod);

		std::string_view getModelPath();

		void normalizeModel();

		int mLODs = 0;

		void calculateLODs();
		void toModelObjHelper(MeshNode* root, int lod, ModelObj& res);

		MeshNode mMeshTree;
		std::string mModelPath = "";
	};
}
