#include "SceneFloor.h"

#include "Renderer.h"
#include "ShaderController.h"
#include "glm.hpp"
#include "Mesh.h"

SceneFloor::SceneFloor() : sceneFloorShader(SHADER_CONTROLLER->loadShader("shaders/floorGrid.vs", "shaders/floorGrid.fs")){
	sceneFloorShader->use();
	constexpr float size = 100.f;
	sceneFloorShader->setVec2("coordShift", {size, size});

    std::vector<GameEngine::Render::Vertex> verticesVec{
		{{-size, 0.0f, -size},{0.f,1.f,0.f},{}},//near left
		{{ size, 0.0f, -size},{0.f,1.f,0.f},{}},//near right
		{{-size, 0.0f,  size},{0.f,1.f,0.f},{}},//far left
		{{ size, 0.0f,  size},{0.f,1.f,0.f},{}},//far right
    };

    std::vector<unsigned> indices{
		0, 1, 2,
    	1, 2, 3
    };
    std::vector<GameEngine::Render::MeshTexture> tex;

    mesh = new GameEngine::Render::Mesh(verticesVec, indices, tex);
}

void SceneFloor::draw(const glm::mat4& ProjectView) {
	sceneFloorShader->use();
	sceneFloorShader->setMat4("PVM", ProjectView);
    mesh->Draw(sceneFloorShader);
	SHADER_CONTROLLER->useDefaultShader();
}

SceneFloor::~SceneFloor() {
	delete mesh;
}
