#pragma once

#include <mutex>
#include <unordered_map>
#include <assimp/material.h>
#include <assimp/mesh.h>

#include "containersModule/Singleton.h"
#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/Model.h"
#include "Animation.h"
#include "assetsModule/TextureHandler.h"

struct aiMaterial;
struct aiMesh;
struct aiScene;
struct aiNode;

namespace AssetsModule {
	class ModelLoader : public SFE::Singleton<ModelLoader> {
	public:
		static SFE::Math::Mat4 assimpMatToMat4(const aiMatrix4x4& from);

		AssetsModule::Model* load(const std::string& path);
		std::map<std::string, std::condition_variable> loading;
		std::mutex mtx;
	private:
		static std::tuple<SFE::Tree<SFE::MeshObject3D>, Armature> loadModel(const aiScene* scene, const std::string& path);

		static int extractLodLevel(const std::string& meshName);

		static void processNode(aiNode* node, const aiScene* scene, const std::string& directory, SFE::Tree<SFE::MeshObject3D>& meshes, Armature& armature);
		static void processMesh(aiMesh* assimpMesh, const aiScene* scene, const std::string& directory, SFE::MeshObject3D& rawModel, Armature& armature);

		static void readBonesData(std::vector<SFE::Vertex3D>& vertices, aiMesh* mesh, const aiScene* scene, Armature& armature);
		static void readMaterialData(SFE::Material& material, aiMaterial* assimpMaterial, const std::string& directory);
		static void readIndicesData(std::vector<unsigned>& vector, unsigned numFaces, aiFace* faces);
		static void readVerticesData(std::vector<SFE::Vertex3D>& vector, unsigned numVertices, aiMesh* aiMesh);

		static std::vector<Texture*> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& directory);
	};
}
