#include "Model.h"

#include <ranges>

#include "renderModule/TextureHandler.h"

using namespace GameEngine::ModelModule;


const std::vector<std::unique_ptr<Mesh>>& Model::getMeshes(size_t LOD) {
	if (meshes.contains(LOD)) {
		return meshes[LOD];
	}

	size_t maxLod = 0;
	for (const auto key : meshes | std::views::keys) {
		maxLod = std::max(key, maxLod);
	}

	return meshes[maxLod];
}

const std::vector<ModelTexture>& Model::getTextures() {
	return textures;
}
