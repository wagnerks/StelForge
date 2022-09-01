#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"


namespace GameEngine::ModelModule {
	struct RawModel {
		std::unordered_map<size_t, std::vector<std::unique_ptr<Mesh>>> meshes;
	};

	class Model {
	public:
		Model(RawModel& model) : meshes(std::move(model.meshes)) {
		}
		const std::vector<std::unique_ptr<Mesh>>& getMeshes(size_t LOD = 0);
	private:
		std::unordered_map<size_t, std::vector<std::unique_ptr<Mesh>>> meshes;
	};
}
