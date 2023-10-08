#pragma once

#include <mutex>
#include <unordered_map>
#include <assimp/material.h>

#include "core/Singleton.h"
#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/Model.h"


struct aiMaterial;
struct aiMesh;

namespace Engine {
	namespace RenderModule {
		class TextureLoader;
	}
}

struct aiScene;
struct aiNode;

namespace AssetsModule {



	class ModelLoader : public Engine::Singleton<ModelLoader> {
		friend Singleton;
	public:
		AssetsModule::Model* load(const std::string& path);
		void releaseModel(const std::string& path);
		std::map<std::string, std::condition_variable> loading;
		std::mutex mtx;
	private:
		ModelLoader() = default;
		~ModelLoader() override;
		void init() override;

		static AssetsModule::MeshNode loadModel(const std::string& path);
		static void processNode(aiNode* node, const aiScene* scene, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::MeshNode& rawModel);
		static void processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, AssetsModule::TextureHandler* loader, const std::string& directory, AssetsModule::MeshNode& rawModel);
		static std::vector<AssetsModule::MaterialTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, AssetsModule::TextureHandler* loader, const std::string& directory);
	};
}
