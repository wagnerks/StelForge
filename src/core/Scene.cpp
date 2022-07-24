#include "Scene.h"

#include "Camera.h"
#include "Engine.h"
#include "componentsModule/MeshComponent.h"
#include "debugModule/ComponentsDebug.h"
#include "modelModule/MeshFactory.h"
#include "shaderModule/ShaderController.h"

using namespace GameModule::CoreModule;

void Scene::init() {
	rootNode = new GameEngine::NodeModule::Node("root");

	skybox = new GameEngine::RenderModule::Skybox("skybox/");
	skybox->init();

	gridFloor = new GameEngine::RenderModule::SceneGridFloor(100.f);
	gridFloor->init();

	auto box = new GameEngine::NodeModule::Node("box1");
	box->getComponent<MeshComponent>()->setMesh(GameEngine::ModelModule::MeshFactory::createPrimitiveMesh(GameEngine::ModelModule::eDrawObjectType::CUBE));
	rootNode->addElement(box);
}

void Scene::updateScene(float dt) {
	GameEngine::Debug::ComponentsDebug::transformComponentDebug(rootNode->getId(), rootNode->getComponent<TransformComponent>());
	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		
		GameEngine::Debug::ComponentsDebug::transformComponentDebug(node->getId(), nodeTC);
		nodeTC->reloadTransform();
	}
}

void Scene::drawScene() {
	auto simpleLight = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/light.vs", "shaders/light.fs");
	simpleLight->use();
	simpleLight->setMat4("projection", GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix());
	simpleLight->setMat4("view", GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix());
	
	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		simpleLight->setMat4("model", nodeTC->getTransform());
		node->getComponent<MeshComponent>()->getMesh()->draw(simpleLight);
	}

	skybox->draw();
	gridFloor->draw();
}

Scene::~Scene() {
	delete rootNode;
	delete skybox;
	delete gridFloor;
}
