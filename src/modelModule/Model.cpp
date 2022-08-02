#include "Model.h"

using namespace GameEngine::ModelModule;

void Model::draw(ShaderModule::ShaderBase* shader, bool ignoreTex) {
    for(auto& mesh : meshes) {
		mesh->draw(shader, ignoreTex);
	}
}

const std::vector<std::unique_ptr<Mesh>>& Model::getMeshes() {
	return meshes;
}

void Model::preDraw(ShaderModule::ShaderBase* currentShader) {
	
}