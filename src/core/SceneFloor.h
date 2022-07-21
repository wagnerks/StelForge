#pragma once
#include <fwd.hpp>

#include "modelModule/Mesh.h"
#include "shader.h"


class SceneFloor {
public:
	SceneFloor();
	void draw(const glm::mat4& ProjectView);
	~SceneFloor();
private:
	GameEngine::ModelModule::Mesh* mesh = nullptr;
	GameEngine::Render::Shader* sceneFloorShader = nullptr;
};
