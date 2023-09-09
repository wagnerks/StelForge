#include "Model.h"

namespace AssetsModule {
	Model::Model(MeshNode& model, std::string_view modelPath) {
		mMeshTree = std::move(model);
		mModelPath = modelPath;
		calculateLODs();
	}

	std::vector<ModelObj> Model::getAllLODs() {
		std::vector<ModelObj> res;
		for (auto lod = 0; lod < mLODs; lod++) {
			res.emplace_back(toModelObj(lod));
		}

		return res;
	}

	ModelObj Model::toModelObj(int lod) {
		ModelObj res;

		toModelObjHelper(&mMeshTree, lod, res);

		return res;
	}

	void Model::toModelObjHelper(MeshNode* root, int lod, ModelObj& res) {
		auto curLod = std::min(lod, static_cast<int>(root->mMeshes.size() - 1));
		if (curLod >= 0) {
			for (auto& mesh : root->mMeshes[curLod]) {
				res.mMeshHandles.emplace_back(mesh);
			}
		}

		for (auto node : root->getElements()) {
			toModelObjHelper(node, lod, res);
		}
	}

	std::string_view Model::getModelPath() {
		return mModelPath;
	}

	void Model::calculateLODs() {
		for (auto node : mMeshTree.getAllNodes()) {
			mLODs = std::max(static_cast<int>(node->mMeshes.size()), mLODs);
		}
	}
}

