#pragma once

#include <mutex>
#include <unordered_map>
#include <assimp/material.h>
#include <assimp/mesh.h>

#include "core/Singleton.h"
#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/Model.h"
#include "Animation.h"
struct aiMaterial;
struct aiMesh;

namespace SFE {
	namespace Render {
		class TextureLoader;
	}
}

struct aiScene;
struct aiNode;

namespace AssetsModule {



	class ModelLoader : public SFE::Singleton<ModelLoader> {
		friend Singleton;
	public:
		static SFE::Math::Mat4 assimpMatToMat4(const aiMatrix4x4& from);

		AssetsModule::Model* load(const std::string& path);
		void releaseModel(const std::string& path);
		std::map<std::string, std::condition_variable> loading;
		std::mutex mtx;
	private:
		ModelLoader() = default;
		~ModelLoader() override;
		void init() override;

		static std::pair<SFE::Tree<Mesh>, Armature> loadModel(const aiScene* scene, const std::string& path);
		static void processNode(aiNode* node, const aiScene* scene, AssetsModule::TextureHandler* loader, const std::string& directory, SFE::Tree<Mesh>& rawModel, Armature& armature);
		static void readBonesData(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene, Armature& armature);
		static void readMaterialData(Material& material, aiMaterial* assimpMaterial, AssetsModule::TextureHandler* loader, const std::string& directory);
		static void readIndicesData(std::vector<unsigned>& vector, unsigned numFaces, aiFace* faces);
		static void readVerticesData(std::vector<Vertex>& vector, unsigned numVertices, aiMesh* aiMesh);
		static int extractLodLevel(const std::string& meshName);
		static void processMesh(aiMesh* assimpMesh, const aiScene* scene, aiNode* meshNode, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::Mesh& rawModel, Armature& armature);
		static std::vector<Texture*> loadMaterialTextures(aiMaterial* mat, aiTextureType type, AssetsModule::TextureHandler* loader, const std::string& directory);
	};
}
