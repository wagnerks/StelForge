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
	class ModelLoader {
	public:
		static ModelLoader* getInstance() {
			if (!instance) {
				instance = new ModelLoader();
			}
			return instance;
		}

		ModelModule::Model* load(const std::string& path);

		void releaseModel(const std::string& path);
	private:
		inline static ModelLoader* instance = nullptr;
		ModelLoader() = default;
		~ModelLoader() = default;

		std::unordered_map<std::string, std::unique_ptr<ModelModule::Model>> models;

		static ModelModule::RawModel loadModel(const std::string& path);
		static void processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::RawModel& rawModel);
		static void processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader, const std::string& directory, ModelModule::RawModel& rawModel);
		static std::vector<ModelModule::MaterialTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader, const std::string& directory);
	};
}
