#pragma once
#include <vector>
#include <assimp/scene.h>

#include "Mesh.h"
#include "componentsModule/ComponentHolder.h"
#include "renderModule/TextureHandler.h"

namespace GameEngine::ModelModule {
	class Model : public ComponentsModule::ComponentHolder {
	public:
		Model(const char* path) {
			loadModel(path);
		}

		void Draw(ShaderModule::ShaderBase* shader);
		const std::vector<Mesh>& getMeshes();
	private:
		//model data
		std::vector<Mesh> meshes;
		std::string directory;

		void loadModel(const std::string& path);
		void processNode(aiNode* node, const aiScene* scene, RenderModule::TextureLoader* loader);
		Mesh processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent, RenderModule::TextureLoader* loader);
		std::vector<MeshTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, RenderModule::TextureLoader* loader);
	};
}
