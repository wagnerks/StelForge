#pragma once
#include <fwd.hpp>

#include "Mesh.h"
#include "shader.h"


class SceneFloor {
public:
	SceneFloor();
	void draw(const glm::mat4& ProjectView);
	~SceneFloor();
private:
	GameEngine::Render::Mesh* mesh = nullptr;
	GameEngine::Render::Shader* sceneFloorShader = nullptr;
};
