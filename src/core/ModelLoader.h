#pragma once

#include <unordered_map>
#include <assimp/material.h>

#include "Singleton.h"
#include "modelModule/Model.h"


struct aiMaterial;
struct aiMesh;

namespace Engine {
	namespace RenderModule {
		class TextureLoader;
	}
}

struct aiScene;
struct aiNode;

namespace Engine::CoreModule {
	class ModelLoader : public Singleton<ModelLoader> {
		friend Singleton;
	public:
		ModelModule::Model* load(const std::string& path);
		void releaseModel(const std::string& path);
	private:
		ModelLoader() = default;
		~ModelLoader() override;

		std::vector<std::pair<std::string, ModelModule::Model*>> models;

		static ModelModule::MeshNode loadModel(const std::string& path);
		static void processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::MeshNode& rawModel);
		static void processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::MeshNode& rawModel);
		static std::vector<ModelModule::MaterialTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory);
	};
}
