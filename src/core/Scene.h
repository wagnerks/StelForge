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

	};
}

