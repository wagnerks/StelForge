#include "Scene.h"

#include "Camera.h"
#include "Engine.h"
#include "componentsModule/MeshComponent.h"
#include "debugModule/ComponentsDebug.h"
#include "modelModule/MeshFactory.h"
#include "shaderModule/ShaderController.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"

#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>
#include <detail/type_quat.hpp>
#include <ext/matrix_transform.hpp>
#include <ext/quaternion_trigonometric.hpp>
#include <gtx/quaternion.hpp>
#include "mat4x4.hpp"
#include "ModelLoader.h"
#include "nodeModule/Node.h"

using namespace GameModule::CoreModule;

void Scene::init() {
	//rootNode = new GameEngine::NodeModule::Node("root");

	skybox = new GameEngine::RenderModule::Skybox("skybox/");
	skybox->init();

	gridFloor = new GameEngine::RenderModule::SceneGridFloor(100.f);
	gridFloor->init();

	/*auto box = new GameEngine::NodeModule::Node("box1");
	box->getComponent<MeshComponent>()->setMesh(GameEngine::ModelModule::MeshFactory::createPrimitiveMesh(GameEngine::ModelModule::eDrawObjectType::CUBE));
	rootNode->addElement(box);
	box->getComponent<TransformComponent>()->setY(2.f);

	auto box2 = new GameEngine::NodeModule::Node("box2");
	box2->getComponent<MeshComponent>()->setMesh(GameEngine::ModelModule::MeshFactory::createPrimitiveMesh(GameEngine::ModelModule::eDrawObjectType::CUBE));
	box2->getComponent<TransformComponent>()->setScale({5.f,1.f,5.f});
	box2->getComponent<TransformComponent>()->setY(-0.9f);*/

	//rootNode->addElement(box2);

	


	modelObj = GameEngine::CoreModule::ModelLoader::getInstance()->load("model/scene.gltf");


    objectPositions.push_back(glm::vec3(-3.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5, -3.0));
    objectPositions.push_back(glm::vec3(-3.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5,  0.0));
    objectPositions.push_back(glm::vec3(-3.0,  3.5,  3.0));
    objectPositions.push_back(glm::vec3( 0.0,  3.5,  3.0));
    objectPositions.push_back(glm::vec3( 3.0,  3.5,  3.0));

}

void Scene::updateScene(float dt) {
	/*GameEngine::Debug::ComponentsDebug::transformComponentDebug(rootNode->getId(), rootNode->getComponent<TransformComponent>());
	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		
		GameEngine::Debug::ComponentsDebug::transformComponentDebug(node->getId(), nodeTC);
		nodeTC->reloadTransform();
	}*/

	//GameEngine::Debug::ComponentsDebug::transformComponentDebug("light", sunLight->getComponent<TransformComponent>());
	//sunLight->getComponent<TransformComponent>()->reloadTransform();
}

void Scene::drawScene() {

	skybox->draw();
	//gridFloor->draw();
}

Scene::~Scene() {
	//delete rootNode;
	delete skybox;
	delete gridFloor;
	//delete sunLight;
}
