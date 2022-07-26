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
#include "nodeModule/Node.h"

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
	box->getComponent<TransformComponent>()->setY(2.f);

	auto box2 = new GameEngine::NodeModule::Node("box2");
	box2->getComponent<MeshComponent>()->setMesh(GameEngine::ModelModule::MeshFactory::createPrimitiveMesh(GameEngine::ModelModule::eDrawObjectType::CUBE));
	box2->getComponent<TransformComponent>()->setScale({5.f,1.f,5.f});
	box2->getComponent<TransformComponent>()->setY(-0.9f);

	rootNode->addElement(box2);

	sunLight = new GameEngine::LightsModule::DirectionalLight();
	sunLight->getComponent<TransformComponent>()->setScale({5.f,5.f,1.f});
	sunLight->getComponent<TransformComponent>()->setY(20.f);
	sunLight->getComponent<TransformComponent>()->setRotate({90.f,0.f,0.f});
	


	modelObj = new GameEngine::ModelModule::Model("model/scene.gltf");

	modelObj->getComponent<TransformComponent>()->setScale({0.03f,0.03f,0.03f});

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
	GameEngine::Debug::ComponentsDebug::transformComponentDebug(rootNode->getId(), rootNode->getComponent<TransformComponent>());
	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		
		GameEngine::Debug::ComponentsDebug::transformComponentDebug(node->getId(), nodeTC);
		nodeTC->reloadTransform();
	}

	GameEngine::Debug::ComponentsDebug::transformComponentDebug("light", sunLight->getComponent<TransformComponent>());
	sunLight->getComponent<TransformComponent>()->reloadTransform();
}

void Scene::drawScene() {
	glm::mat4 projection = GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix();
    glm::mat4 view = GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix();
	auto camera = GameEngine::Engine::getInstance()->getCamera();

	auto simpleDepthShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/depth.vs", "shaders/depth.fs");
	auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/objectWithShadow.vs", "shaders/objectWithShadow.fs");
    shader->use();

	unsigned int woodTexture = GameEngine::RenderModule::TextureHandler::getInstance()->loader.loadTexture("test.png");
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE5, GL_TEXTURE_2D, woodTexture);
    shader->setInt("diffuseTexture", 5);
    shader->setInt("shadowMap", 1);

	
	
	sunLight->preDraw();


	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		simpleDepthShader->setMat4("model", nodeTC->getTransform());
		node->getComponent<MeshComponent>()->getMesh()->draw(simpleDepthShader);
	}

	glm::mat4 model = glm::mat4(1.0f);
	for (unsigned int i = 0; i < objectPositions.size(); i++) {
		model = glm::mat4(1.0f);
		model = glm::translate(model, objectPositions[i]);
		model = glm::scale(model, glm::vec3(0.005f));
		simpleDepthShader->setMat4("model", model);
		modelObj->draw(simpleDepthShader, true);
	}

	sunLight->postDraw();



    shader->use();



    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    // set light uniforms
    shader->setVec3("viewPos", GameEngine::Engine::getInstance()->getCamera()->Position);
    shader->setVec3("lightPos", sunLight->getComponent<TransformComponent>()->getPos());
    shader->setMat4("lightSpaceMatrix", sunLight->getLightSpaceProjection());
	
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, sunLight->getDepthMapTexture());

	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		shader->setMat4("model", nodeTC->getTransform());
		node->getComponent<MeshComponent>()->getMesh()->draw(shader);
	}

	auto scale = sunLight->getComponent<TransformComponent>()->getScale();
	sunLight->getComponent<TransformComponent>()->setScale({0.5f,0.5f,0.5f});
	sunLight->getComponent<TransformComponent>()->reloadTransform();
	shader->setMat4("model", sunLight->getComponent<TransformComponent>()->getTransform());
	GameEngine::RenderModule::Utils::renderCube();
	sunLight->getComponent<TransformComponent>()->setScale(scale);
	sunLight->getComponent<TransformComponent>()->reloadTransform();

	auto lines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
	lines->use();
	lines->setMat4("PVM",GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix() * GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix() *  sunLight->getComponent<TransformComponent>()->getTransform());
	GameEngine::RenderModule::Utils::renderXYZ(20.f);

	
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, sunLight->getDepthMapTexture());
	auto depthDebugShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugDepth.vs", "shaders/debugDepth.fs");
	depthDebugShader->use();
	depthDebugShader->setInt("depthMap", 1);
	depthDebugShader->setFloat("near_plane", sunLight->getNearPlane());
	depthDebugShader->setFloat("far_plane", sunLight->getFarPlane());
	GameEngine::RenderModule::Utils::renderQuad(0.7f, -0.2f,1.0f,0.1f);

	/*GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, gPosition);
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, gNormal);
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, gAlbedoSpec);*/
	/*auto drawImageShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/imageDraw.vs", "shaders/imageDraw.fs");

	drawImageShader->use();
	drawImageShader->setInt("tex", 0);
	GameEngine::RenderModule::Utils::renderQuad(0.7f, 0.7f,1.f,1.f);*/


	


	auto drawImageShaderDT = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/imageDrawDepthTransform.vs", "shaders/imageDrawDepthTransform.fs");
	drawImageShaderDT->use();
	drawImageShaderDT->setInt("tex", 2);
	drawImageShaderDT->setInt("uPos", 0);
	drawImageShaderDT->setMat4("lightSpaceMatrix", sunLight->getLightSpaceProjection());

	auto cameraRotationQuat = glm::toMat4(glm::quat({glm::radians(camera->Pitch), glm::radians(camera->Yaw),glm::radians(0.f)}));
	auto camerTransform = glm::translate(glm::mat4(1.0f), camera->Position) *  cameraRotationQuat * glm::scale(glm::mat4(1.0f), glm::vec3(1.f,1.f,1.f));
	drawImageShaderDT->setMat4("model", camerTransform);
	drawImageShaderDT->setMat4("proj", projection);
	drawImageShaderDT->setMat4("view", view);
    drawImageShaderDT->setVec3("lightPos", sunLight->getComponent<TransformComponent>()->getPos());

	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, sunLight->getDepthMapTexture());

	GameEngine::RenderModule::Utils::renderQuad(0.7f, -0.5f,1.0f,-0.2f);

	skybox->draw();
	gridFloor->draw();
}

Scene::~Scene() {
	delete rootNode;
	delete skybox;
	delete gridFloor;
	delete sunLight;
}
