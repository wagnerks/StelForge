#pragma once
#include <fwd.hpp>

#include "modelModule/Mesh.h"

class SceneFloor {
public:
	SceneFloor();
	void draw(const glm::mat4& ProjectView);
	~SceneFloor();
private:
	GameEngine::ModelModule::Mesh* mesh = nullptr;
	GameEngine::ShaderModule::Shader* sceneFloorShader = nullptr;
};
