#pragma once
#include <memory>
#include <vector>

#include "Mesh.h"


namespace GameEngine::ModelModule {
	class Model {
	public:
		Model(std::vector<std::unique_ptr<Mesh>>& meshes) : meshes(std::move(meshes)) {
		}

		void draw(ShaderModule::ShaderBase* shader, bool ignoreTex = false);
		const std::vector<std::unique_ptr<Mesh>>& getMeshes();

		void preDraw(ShaderModule::ShaderBase* currentShader);
	private:
		std::vector<std::unique_ptr<Mesh>> meshes;
	};
}
