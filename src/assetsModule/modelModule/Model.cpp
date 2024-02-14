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

		for (auto node : root->getAllNodes()) {
			if (node->armature.getBones().size() > mBones.size()) {
				mBones.clear();
				auto i = 0;
				for (auto& bone : node->armature.getBones()) {
					mBones[bone.name] = BoneInfo{i, bone.offset};
					i++;
				}
			}
		}

		for (auto node : root->getElements()) {
			toModelObjHelper(node, lod, res);
		}
	}

	std::string_view Model::getModelPath() {
		return mModelPath;
	}

	void Model::normalizeModel(bool smooth) {
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
			const auto Normal = SFE::Math::normalize(SFE::Math::cross(AB, AC));

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
			const auto divisor = (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
			const float r = divisor ? 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x) : 1.f;

			SFE::Math::Vec3 T = SFE::Math::normalize((AB * deltaUV2.y - AC * deltaUV1.y) * r); //tangent
			// re-orthogonalize T with respect to N
			T = SFE::Math::normalize(T - Normal * SFE::Math::dot(T, Normal));
			
			// then retrieve perpendicular vector biTangent with the cross product of T and N
			auto biTangent = SFE::Math::normalize(SFE::Math::cross(Normal, T));

			mesh.mData.mVertices[a].mTangent = T;
			mesh.mData.mVertices[b].mTangent = T;
			mesh.mData.mVertices[c].mTangent = T;

			mesh.mData.mVertices[a].mBiTangent = biTangent;
			mesh.mData.mVertices[b].mBiTangent = biTangent;
			mesh.mData.mVertices[c].mBiTangent = biTangent;

		};		

		for (auto node : mMeshTree.getAllNodes()) {
			for (auto& lods : node->mMeshes) {
				for (auto& mesh : lods) {
					for (int i = 2; i < mesh.mData.mIndices.size(); i += 3) {
						normalizeTriangle(mesh, mesh.mData.mIndices[i - 2], mesh.mData.mIndices[i - 1], mesh.mData.mIndices[i]);
					}

					if (smooth) {
						std::map<SFE::Math::Vec3, std::vector<Vertex*>> vertices;
						for (auto& vertex : mesh.mData.mVertices) {
							vertices[vertex.mPosition].push_back(&vertex);
						}

						std::vector<std::vector<Vertex*>> verticesWithOppositTangents;
						verticesWithOppositTangents.emplace_back();
						for (auto [_, copyVertices] : vertices) {
							SFE::Math::Vec3 normal;
							SFE::Math::Vec3 tangent;
							std::vector<Vertex*> nears;
							nears.reserve(copyVertices.size());

							while (!copyVertices.empty()) {
								normal = {};
								tangent = {};

								nears.clear();

								if (!verticesWithOppositTangents.back().empty()) {
									verticesWithOppositTangents.emplace_back();
								}

								for (auto vertex : copyVertices) {
									const auto dotProductNormal = dot(vertex->mNormal, (copyVertices.back())->mNormal);
									//if dot product <= 0 -> angle >= 90 -> do not sum this normals, to avoid smooth cube, or round sword
									if (std::fabs(dotProductNormal) > std::numeric_limits<float>::epsilon()) {//use back to optimize erasing
										const auto dotProductTangent = dot(vertex->mTangent, (copyVertices.back())->mTangent);//avoid smoothing opposite tangents
										if (dotProductTangent > std::numeric_limits<float>::epsilon()) {
											nears.push_back(vertex);
											normal += vertex->mNormal;
											tangent += vertex->mTangent;
										}
										verticesWithOppositTangents.back().push_back(vertex);
									}
								}

								if (nears.size() <= 1) { //if nears size == 1 - it means that dot was > 0.f only one time, which possible only if there is no near vertex and algorithm found only copyVertices.back()
									copyVertices.pop_back();
									continue;
								}

								std::erase_if(copyVertices, [nears](Vertex* vert) {
									return std::find(nears.begin(), nears.end(), vert) != nears.end();
								});

								normal = normal / nears.size(); //normalize - we know that normal was summed size() times
								tangent = tangent / nears.size();

								tangent = tangent - normal * SFE::Math::dot(tangent, normal);
								auto biTangent = SFE::Math::cross(normal, tangent);//recalculate bitangent to be sure it is perpendicular to normal-tangent plane
								for (auto vertex : nears) {
									vertex->mNormal = normal;
									vertex->mTangent = tangent;
									vertex->mBiTangent = biTangent;
								}
							}
						}

						for (auto& oppositeVertices : verticesWithOppositTangents) {
							SFE::Math::Vec3 normal;

							for (auto vertex : oppositeVertices) {
								normal += vertex->mNormal;
							}

							normal = normal / oppositeVertices.size();
							for (auto vertex : oppositeVertices) {
								vertex->mNormal = normal;
							}
						}
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

