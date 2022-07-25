#include "Scene.h"

#include "Camera.h"
#include "Engine.h"
#include "componentsModule/MeshComponent.h"
#include "debugModule/ComponentsDebug.h"
#include "modelModule/MeshFactory.h"
#include "shaderModule/ShaderController.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"

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

	auto box2 = new GameEngine::NodeModule::Node("box2");
	box2->getComponent<MeshComponent>()->setMesh(GameEngine::ModelModule::MeshFactory::createPrimitiveMesh(GameEngine::ModelModule::eDrawObjectType::CUBE));
	box2->getComponent<TransformComponent>()->setScale({5.f,1.f,5.f});
	box2->getComponent<TransformComponent>()->setY(-1.f);

	rootNode->addElement(box2);

	sunLight = new GameEngine::LightsModule::DirectionalLight();
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
	auto simpleDepthShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/depth.vs", "shaders/depth.fs");
	auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/objectWithShadow.vs", "shaders/objectWithShadow.fs");
    shader->use();
    shader->setInt("diffuseTexture", 0);
    shader->setInt("shadowMap", 1);

	unsigned int woodTexture = GameEngine::RenderModule::TextureHandler::getInstance()->loader.loadTexture("test.png");
	
	sunLight->preDraw();


	for (auto node : rootNode->getAllNodes()) {
		auto nodeTC = node->getComponent<TransformComponent>();
		simpleDepthShader->setMat4("model", nodeTC->getTransform());
		node->getComponent<MeshComponent>()->getMesh()->draw(simpleDepthShader);
	}

	sunLight->postDraw();


    shader->use();

    glm::mat4 projection = GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix();
    glm::mat4 view = GameEngine::Engine::getInstance()->getCamera()->GetViewMatrix();

    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    // set light uniforms
    shader->setVec3("viewPos", GameEngine::Engine::getInstance()->getCamera()->Position);
    shader->setVec3("lightPos", sunLight->getComponent<TransformComponent>()->getPos());
    shader->setMat4("lightSpaceMatrix", sunLight->getLightSpaceProjection());
	GameEngine::RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, woodTexture);
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
	auto drawImageShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugDepth.vs", "shaders/debugDepth.fs");
	drawImageShader->use();
	drawImageShader->setInt("depthMap", 1);
	drawImageShader->setFloat("near_plane", sunLight->getNearPlane());
	drawImageShader->setFloat("far_plane", sunLight->getFarPlane());
	GameEngine::RenderModule::Utils::renderQuad(0.7f, -0.2f,1.0f,0.1f);

	skybox->draw();
	gridFloor->draw();
}

Scene::~Scene() {
	delete rootNode;
	delete skybox;
	delete gridFloor;
	delete sunLight;
}
