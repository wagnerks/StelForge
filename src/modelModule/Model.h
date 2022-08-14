#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"


namespace GameEngine::ModelModule {
	struct ModelTexture {
		unsigned int id = 0;
		std::string type;
	};

	struct RawModel {
		std::unordered_map<size_t, std::vector<std::unique_ptr<Mesh>>> meshes;
		 
		std::vector<ModelTexture> textures;
	};

	class Model {
	public:
		Model(RawModel& model) : meshes(std::move(model.meshes)), textures(model.textures) {
		}
		const std::vector<std::unique_ptr<Mesh>>& getMeshes(size_t LOD = 0);
		const std::vector<ModelTexture>& getTextures();
	private:
		std::unordered_map<size_t, std::vector<std::unique_ptr<Mesh>>> meshes;
		std::vector<ModelTexture> textures;
	};
}
