#include "Scene.h"

using namespace GameModule::CoreModule;

void Scene::init() {
	rootNode = new GameEngine::NodeModule::Node("root");

	skybox = new GameEngine::RenderModule::Skybox("skybox/");
	skybox->init();

	gridFloor = new GameEngine::RenderModule::SceneGridFloor(100.f);
	gridFloor->init();
}
void Scene::updateScene(float dt) {
	
}
void Scene::drawScene() {

	skybox->draw();
	gridFloor->draw();
}

Scene::~Scene() {
	delete rootNode;
	delete skybox;
	delete gridFloor;
}
