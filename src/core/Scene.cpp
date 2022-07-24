#include "Scene.h"

using namespace GameModule::CoreModule;

void Scene::init() {
	rootNode = new GameEngine::NodeModule::Node("root");
	skybox = new GameEngine::RenderModule::Skybox("skybox/");
	skybox->init();
}
void Scene::updateScene(float dt) {
	
}
void Scene::drawScene() {
	skybox->draw();
}
