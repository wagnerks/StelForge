#include "Model.h"

namespace AssetsModule {
	Model::Model(MeshNode model, std::string_view modelPath) {
		mMeshTree = std::move(model);
		mModelPath = modelPath;
		calculateLODs();
		getAllLODs();
	}

	std::vector<ModelObj>* Model::getAllLODs() {
		if (lods.empty()) {
			for (auto lod = 0; lod < mLODs; lod++) {
				lods.emplace_back(toModelObj(lod));
			}
		}

		return &lods;
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

	void Model::normalizeModel() {
		/*if (normalized) {
			return;
		}*/

		normalized = true;
		auto normalizeTriangle = [](Mesh& mesh, int a, int b, int c) {
			const auto& A = mesh.mData.mVertices[a].mPosition;
			const auto& B = mesh.mData.mVertices[b].mPosition;
			const auto& C = mesh.mData.mVertices[c].mPosition;

			const auto AB = B - A;
			const auto AC = C - A;

			//Normal of ABC triangle
			const auto Normal = Engine::Math::normalize(Engine::Math::cross(AB, AC));
			mesh.mData.mVertices[a].mNormal = Normal;
			mesh.mData.mVertices[b].mNormal = Normal;
			mesh.mData.mVertices[c].mNormal = Normal;

			// Shortcuts for UVs
			const auto& uv0 = mesh.mData.mVertices[a].mTexCoords;
			const auto& uv1 = mesh.mData.mVertices[b].mTexCoords;
			const auto& uv2 = mesh.mData.mVertices[c].mTexCoords;

			// UV delta
			const auto deltaUV1 = uv0 - uv1;
			const auto deltaUV2 = uv0 - uv2;

			const float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

			Engine::Math::Vec3 T = Engine::Math::normalize((AB * deltaUV2.y - AC * deltaUV1.y) * r); //tangent

			// re-orthogonalize T with respect to N
			T = Engine::Math::normalize(T - Normal * Engine::Math::dot(T, Normal));

			// then retrieve perpendicular vector biTangent with the cross product of T and N
			auto biTangent = Engine::Math::normalize(Engine::Math::cross(Normal, T));

			mesh.mData.mVertices[a].mTangent = T;
			mesh.mData.mVertices[b].mTangent = T;
			mesh.mData.mVertices[c].mTangent = T;

			mesh.mData.mVertices[c].mBiTangent = biTangent;
			mesh.mData.mVertices[c].mBiTangent = biTangent;
			mesh.mData.mVertices[c].mBiTangent = biTangent;

		};

		for (auto node : mMeshTree.getAllNodes()) {
			for (auto& lods : node->mMeshes) {
				for (auto& mesh : lods) {
					for (int i = 2; i < mesh.mData.mIndices.size(); i += 3) {
						normalizeTriangle(mesh, mesh.mData.mIndices[i - 2], mesh.mData.mIndices[i - 1], mesh.mData.mIndices[i]);
					}

					mesh.unbindMesh();
					mesh.bindMesh();
				}
			}
		}
	}

	void Model::bindMeshes() {
		for (auto node : mMeshTree.getAllNodes()) {
			for (auto& lods : node->mMeshes) {
				for (auto& mesh : lods) {
					mesh.unbindMesh();
					mesh.bindMesh();
				}
			}
		}

		lods.clear();
		getAllLODs();
	}

	void Model::calculateLODs() {
		for (auto node : mMeshTree.getAllNodes()) {
			mLODs = std::max(static_cast<int>(node->mMeshes.size()), mLODs);
		}
	}
}

