#pragma once
#include <memory>
#include <unordered_map>
#include <assimp/material.h>

#include "modelModule/Model.h"

struct aiMaterial;
struct aiMesh;

namespace GameEngine {
	namespace RenderModule {
		class TextureLoader;
	}
}

struct aiScene;
struct aiNode;

namespace GameEngine::CoreModule {

	struct ModelWithLODS {
		std::unique_ptr<ModelModule::Model> model;

		std::vector<std::unique_ptr<ModelModule::Model>> LODs;
	};

	class ModelLoader {
	public:
		static ModelLoader* getInstance() {
			if (!instance) {
				instance = new ModelLoader();
			}
			return instance;
		}

		ModelModule::Model* load(const std::string& path);
		ModelModule::Model* loadLOD(const std::string& path);
		void releaseModel(const std::string& path);
	private:
		inline static ModelLoader* instance = nullptr;
		ModelLoader() = default;
		~ModelLoader() = default;

		std::unordered_map<std::string, std::unique_ptr<ModelModule::Model>> models;

		static std::vector<std::unique_ptr<ModelModule::Mesh>> loadModel(const std::string& path);
		static void processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, std::vector<std::unique_ptr<ModelModule::Mesh>>& meshes);
		static std::unique_ptr<ModelModule::Mesh> processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory);
		static std::vector<ModelModule::MeshTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory);
	};
}
