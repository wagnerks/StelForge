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
		// Loads every model not loaded yet, reading the files and decoding their textures in parallel. From the
		// thread the device is used from, like load.
		void loadAll(const std::vector<std::string>& paths);
		std::map<std::string, std::condition_variable> loading;
		std::mutex mtx;
	private:
		// A model between reading its file and becoming an asset.
		struct Loaded {
			SFE::Tree<SFE::MeshObject3D> meshes;
			Armature armature;
			std::vector<Animation> animations;
			bool ok = false;
		};
		// Loading in three steps: read touches the device (textures), finishMeshes is CPU only and may run on any
		// thread, publish uploads the meshes and ends the load.
		static Loaded read(const std::string& path, const aiScene* scene);
		static void finishMeshes(Loaded& loaded);
		AssetsModule::Model* publish(const std::string& path, Loaded&& loaded);
		// The files a texture a material names may be, in the order they are tried.
		static std::vector<std::string> textureFileCandidates(const std::string& raw, const std::string& directory);
		static std::tuple<SFE::Tree<SFE::MeshObject3D>, Armature> loadModel(const aiScene* scene, const std::string& path);

		static int extractLodLevel(const std::string& meshName);

		static void processNode(aiNode* node, const aiScene* scene, const std::string& directory, const std::string& atlasStem, SFE::Tree<SFE::MeshObject3D>& meshes, Armature& armature);
		static void processMesh(aiMesh* assimpMesh, const aiScene* scene, const std::string& directory, const std::string& atlasStem, SFE::MeshObject3D& rawModel, Armature& armature);

		static void readBonesData(std::vector<SFE::Vertex3D>& vertices, aiMesh* mesh, Armature& armature);
		static void finalizeArmature(const aiScene* scene, Armature& armature);
		static void readMaterialData(SFE::Material& material, aiMaterial* assimpMaterial, const std::string& directory, const std::string& atlasStem, const std::string& meshName);
		static void readIndicesData(std::vector<unsigned>& vector, unsigned numFaces, aiFace* faces);
		static void readVerticesData(std::vector<SFE::Vertex3D>& vector, unsigned numVertices, aiMesh* aiMesh);

		static std::vector<Texture*> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& directory);
	};
}
