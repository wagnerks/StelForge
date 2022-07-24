#pragma once
#include "nodeModule/Node.h"
#include "renderModule/Skybox.h"

namespace GameModule::CoreModule {
	class Scene {
	public:
		void init();
		void updateScene(float dt);
		void drawScene();
	private:
		GameEngine::NodeModule::Node* rootNode = nullptr;
		GameEngine::RenderModule::Skybox* skybox = nullptr;
	};
}

