#pragma once
#include <vector>
#include <assimp/scene.h>

#include "ComponentHolder.h"
#include "Mesh.h"

namespace GameEngine::Render {
	class Model : public ComponentsModule::ComponentHolder {
	public:
		Model(const char* path) {
			loadModel(path);
		}

		void Draw(Shader* shader);
	private:
		//model data
		std::vector<MeshTexture> textures_loaded;
		std::vector<Mesh> meshes;
		std::string directory;

		void loadModel(const std::string& path);
		void processNode(aiNode* node, const aiScene* scene);
		Mesh processMesh(aiMesh* mesh, const aiScene* scene, aiNode* parent);
		std::vector<MeshTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
	};
}
