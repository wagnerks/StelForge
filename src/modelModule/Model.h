#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"
#include "assetsModule/Asset.h"


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
			if (this == &other) {
				return *this;
			}

			TreeNode<MeshNode>::operator =(std::move(other));
			mMeshes = std::move(other.mMeshes);
			return *this;
		}

		std::vector<std::vector<Mesh>> mMeshes;
	};

	class Model : public Asset {
	public:
		Model(MeshNode& model, std::string_view modelPath) {
			mMeshTree = std::move(model);
			mModelPath = modelPath;
			calculateLODs();
		}

		~Model() override {}

		std::vector<ModelObj> getAllLODs() {
			std::vector<ModelObj> res;
			for (auto lod = 0; lod < mLODs; lod++) {
				res.emplace_back(toModelObj(lod));
			}

			return res;
		}

		ModelObj toModelObj(int lod) {
			ModelObj res;

			toModelObjHelper(&mMeshTree, lod, res);

			return res;
		};

		void toModelObjHelper(MeshNode* root, int lod, ModelObj& res) {
			auto curLod = std::min(lod, static_cast<int>(root->mMeshes.size() - 1));
			if (curLod >= 0) {
				for (auto& mesh : root->mMeshes[curLod]) {
					res.mMeshHandles.emplace_back(mesh);
				}
			}

			for (auto node : root->getElements()) {
				toModelObjHelper(node, lod, res);
			}
		};



		MeshNode mMeshTree;
		std::string_view getModelPath() {
			return mModelPath;
		}
	private:
		int mLODs = 0;

		void calculateLODs() {
			for (auto node : mMeshTree.getAllNodes()) {
				mLODs = std::max(static_cast<int>(node->mMeshes.size()), mLODs);
			}
		}

		std::string mModelPath = "";
	};
}
