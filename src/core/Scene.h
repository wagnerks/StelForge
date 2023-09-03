#pragma once
#include "modelModule/Model.h"
#include "renderModule/DirectionalOrthoLight.h"
#include "renderModule/SceneGridFloor.h"
#include "renderModule/Skybox.h"

namespace GameModule::CoreModule {
	class Scene {
	public:
		void init();
		void updateScene(float dt);
		void drawScene();
		~Scene();
	private:
		//GameEngine::NodeModule::Node* rootNode = nullptr;
		Engine::RenderModule::Skybox* skybox = nullptr;
		Engine::RenderModule::SceneGridFloor* gridFloor = nullptr;
		Engine::LightsModule::DirectionalOrthoLight* sunLight = nullptr;

		std::vector<glm::vec3> objectPositions;

		std::vector<glm::vec3> randomLightSpeeds;

		Engine::ModelModule::Model* modelObj = nullptr;
	};
}

