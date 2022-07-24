#include "Renderer.h"

#include <deque>
#include <iostream>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "Utils.h"
#include "componentsModule/DrawComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "debugModule/ComponentsDebug.h"
#include "logsModule/logger.h"
#include "modelModule/MeshFactory.h"
#include "modelModule/Model.h"
#include "nodeModule/Node.h"
#include "shaderModule/ShaderController.h"
#include "gtc/random.hpp"

constexpr int glfw_context_ver_maj = 3;
constexpr int glfw_context_ver_min = 3;



using namespace GameEngine;
using namespace GameEngine::RenderModule;
using namespace GameEngine::CoreModule;

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	Renderer::SCR_WIDTH = width;
	Renderer::SCR_HEIGHT = height;
}

void Renderer::draw() {
	RenderModule::Renderer::drawCallsCount = 0;
	RenderModule::Renderer::drawVerticesCount = 0;

	//std::vector<glm::mat4> modelMatrices;
	//for (auto backpack : sceneNode->getElement("backpackHolder")->getAllNodes()) {
	//	backpack->getComponent<TransformComponent>()->reloadTransform();
	//	modelMatrices.push_back(backpack->getComponent<TransformComponent>()->getTransform());
	//}

	//auto amount = sceneNode->getElement("backpackHolder")->getAllNodes().size();
	//// vertex buffer object
	//glBindBuffer(GL_ARRAY_BUFFER, buffer);
	//glBufferSubData(GL_ARRAY_BUFFER, 0, amount * sizeof(glm::mat4), &modelMatrices[0]);
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	//glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
	//glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	//glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));

	//glm::mat4 view = Engine::getInstance()->getCamera()->GetViewMatrix();	       
	//glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
	//glBindBuffer(GL_UNIFORM_BUFFER, 0);

	//glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);


	//auto backpacksCount = sceneNode->getElement("backpackHolder")->getAllNodes().size();

	//lights[0]->getComponent<TransformComponent>()->setPos(sceneNode->getElement("light")->getComponent<TransformComponent>()->getPos());
	//lights[0]->getComponent<TransformComponent>()->setRotate(sceneNode->getElement("light")->getComponent<TransformComponent>()->getRotate());
	//lights[0]->getComponent<TransformComponent>()->setScale(sceneNode->getElement("light")->getComponent<TransformComponent>()->getScale());
	//lights[0]->getComponent<TransformComponent>()->reloadTransform();
	//for (auto light : lights) {
	//	// 1. first render to depth map
	//	light->preDraw();
	//	
	//	for (const auto& mesh : modelObj->getMeshes()) {
	//	    glBindVertexArray(mesh.getVAO());
	//	    glDrawElementsInstanced(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0, backpacksCount);
	//		RenderModule::Renderer::drawCallsCount++;
	//		RenderModule::Renderer::drawVerticesCount += mesh.indices.size() * backpacksCount;
	//		glBindVertexArray(0);
	//	}

	//	light->postDraw();
	//

	//	// 2. then render scene as normal with shadow mapping (using depth map)
	//	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//	SHADER_CONTROLLER->defaultShader->use();

	//	SHADER_CONTROLLER->defaultShader->setVec3("lightPos", sceneNode->getElement("light")->getComponent<TransformComponent>()->getPos());
	//	SHADER_CONTROLLER->defaultShader->setVec3("viewPos", Engine::getInstance()->getCamera()->Position);
	//	SHADER_CONTROLLER->defaultShader->setMat4("lightSpaceMatrix", light->getLightSpaceProjection());
	//	

	//    SHADER_CONTROLLER->defaultShader->setInt("diffuseTexture", 0);
	//    SHADER_CONTROLLER->defaultShader->setInt("shadowMap", 1);
	//	auto tex = TextureHandler::getInstance()->loader.loadTexture("skybox/top");
	//	TextureHandler::getInstance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, tex);

	//	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, light->getDepthMapTexture());

	//	for (const auto& mesh : modelObj->getMeshes()) {
	//	    glBindVertexArray(mesh.getVAO());
	//	    glDrawElementsInstanced(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0, backpacksCount);
	//		RenderModule::Renderer::drawCallsCount++;
	//		RenderModule::Renderer::drawVerticesCount += mesh.indices.size() * backpacksCount;
	//		glBindVertexArray(0);
	//	}

	//	
	//}
	//for (auto light : lights) {

	//	
	//	auto lightTC = sceneNode->getElement("light")->getComponent<TransformComponent>();
	//	auto prevPos = lightTC->getPos();
	//	auto prevScale = lightTC->getScale();
	//	
	//	auto rotate = lightTC->getRotate();
	//	

	//	lightTC->setRotate(light->getComponent<TransformComponent>()->getRotate());
	//	lightTC->setPos(light->getComponent<TransformComponent>()->getPos());

	//	sceneNode->getElement("light")->getComponent<DrawComponent>()->draw();
	//	glm::vec3 front = lightTC->getFront();

	//	lightTC->setPos(light->getComponent<TransformComponent>()->getPos() + front);
	//	lightTC->setScale({0.3f,0.3f,0.3f});
	//	lightTC->setRotate({});
	//	sceneNode->getElement("light")->getComponent<DrawComponent>()->draw();
	//	lightTC->setPos(prevPos);
	//	lightTC->setScale(prevScale);
	//	lightTC->setRotate(rotate);
	//}
	
	/*for (auto node : sceneNode->getAllNodes()) {
		node->getComponent<ShaderComponent>()->setShader(SHADER_CONTROLLER->defaultShader);
		node->getComponent<DrawComponent>()->draw();
	}*/

	Debug::ComponentsDebug::transformComponentDebug(sceneNode->getId(), sceneNode->getComponent<TransformComponent>());
	for (auto node : sceneNode->getAllNodes()) {
		Debug::ComponentsDebug::transformComponentDebug(node->getId(), node->getComponent<TransformComponent>());
	}
	

	/*auto debugDepth = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/debugDepth.vs", "shaders/debugDepth.fs");
	debugDepth->use();
    debugDepth->setFloat("near_plane", lights[0]->getNearPlane());
    debugDepth->setFloat("far_plane", lights[0]->getFarPlane());
	debugDepth->setInt("depthMap", 1);
	TextureHandler::getInstance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, lights[0]->getDepthMapTexture());
	Utils::renderQuad();*/

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	auto& projection = Engine::getInstance()->getCamera()->getProjectionsMatrix();
	auto view = Engine::getInstance()->getCamera()->GetViewMatrix();

	sceneNode->getElement("light")->getComponent<TransformComponent>()->reloadTransform();
	auto light = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/light.vs", "shaders/light.fs");
	light->use();
	light->setMat4("projection", projection);
	light->setMat4("view", view);
	light->setMat4("model", sceneNode->getElement("light")->getComponent<TransformComponent>()->getTransform());

	auto mesh = sceneNode->getElement("light")->getComponent<ComponentsModule::MeshComponent>()->getMesh();
	mesh->draw(light);

	auto lightTC = sceneNode->getElement("light")->getComponent<TransformComponent>();
	auto xyzShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
	xyzShader->use();
	xyzShader->setMat4("PVM", projection * view * lightTC->getTransform());

	Utils::renderXYZ(3.f);

	skybox->draw();
	floor->draw();
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	sceneNode = new NodeModule::Node("scene");
	auto kek = sceneNode->getComponent<TransformComponent>()->getTransform();

	floor = new SceneGridFloor(100.f);
	floor->init();

	skybox = new Skybox("skybox/");
	skybox->init();

	auto childNode = new NodeModule::Node("light");
	childNode->getComponent<ComponentsModule::MeshComponent>()->setMesh(ModelModule::MeshFactory::createPrimitiveMesh(ModelModule::eDrawObjectType::CUBE));

	sceneNode->addElement(childNode);
	
	/*modelObj = new ModelModule::Model("suzanne/scene.gltf");

	auto backpacks = new NodeModule::Node("backpackHolder");
	sceneNode->addElement(backpacks);*/

	/*int n =0;
	auto max = 15;
	float dif = 3.f;
	for (auto i = 0; i < max; i++) {
		for (auto j = 0; j < max; j++) {
			for (auto k = 0; k < max; k++) {
				auto backpack = new NodeModule::Node("backpack" + std::to_string(n));
				backpack->getComponent<ModelComponent>()->setModel(modelObj);
				auto tc = backpack->getComponent<TransformComponent>();
				tc->setPos({i * dif, j * dif, k * -dif});
				tc->setRotateX(-90.f);
				backpacks->addElement(backpack);
				n++;
			}
		}
	}*/
	//std::vector<glm::mat4> modelMatrices;
	//for (auto backpack : backpacks->getAllNodes()) {
	//	backpack->getComponent<TransformComponent>()->reloadTransform();
	//	modelMatrices.push_back(backpack->getComponent<TransformComponent>()->getTransform());
	//}

	//auto amount = backpacks->getAllNodes().size();
	//// vertex buffer object
	//
	//glGenBuffers(1, &buffer);
	//glBindBuffer(GL_ARRAY_BUFFER, buffer);
	//glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), modelMatrices.data(), GL_STATIC_DRAW);
	//  
	//for(unsigned int i = 0; i < modelObj->getMeshes().size(); i++)
	//{
	//    unsigned int VAO = modelObj->getMeshes()[i].getVAO();
	//    glBindVertexArray(VAO);
	//	auto vertexOffset = sizeof(ModelModule::Vertex);
	//    // set attribute pointers for matrix (4 times vec4)
 //       glEnableVertexAttribArray(3);
 //       glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
 //       glEnableVertexAttribArray(4);
 //       glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + sizeof(glm::vec4)));
 //       glEnableVertexAttribArray(5);
 //       glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + 2 * sizeof(glm::vec4)));
 //       glEnableVertexAttribArray(6);
 //       glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(0 + 3 * sizeof(glm::vec4)));

 //       glVertexAttribDivisor(3, 1);
 //       glVertexAttribDivisor(4, 1);
 //       glVertexAttribDivisor(5, 1);
 //       glVertexAttribDivisor(6, 1);

	//    glBindVertexArray(0);
	//}  
	//glBindBuffer(GL_ARRAY_BUFFER, 0);

	
	/*
	unsigned int main    = glGetUniformBlockIndex(SHADER_CONTROLLER->loadVertexFragmentShader("shaders/main.vs", "shaders/main.fs")->getID(), "Matrices");
	
	auto geom = SHADER_CONTROLLER->loadGeometryShader("shaders/geometry.vs","shaders/geometry.fs","shaders/geometry.gs");
	unsigned int getomPlace    = glGetUniformBlockIndex(geom->getID(), "Matrices");

	glUniformBlockBinding(SHADER_CONTROLLER->loadVertexFragmentShader("shaders/main.vs", "shaders/main.fs")->getID(),    main, 0);
	glUniformBlockBinding(geom->getID(), getomPlace, 0);

	
	glGenBuffers(1, &uboMatrices);
	  
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	  
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));

	
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f, 100.0f);
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glm::mat4 view = Engine::getInstance()->getCamera()->GetViewMatrix();	       
	glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
	glBindBuffer(GL_UNIFORM_BUFFER, 0); */
}

void Renderer::terminate() const {
	glfwTerminate();
}

void Renderer::drawCall() {
	
}


GLFWwindow* Renderer::initGLFW() {
	if (GLFWInited) {
		assert(false && "GLFW Already inited");
		return nullptr;
	}
	GLFWInited = true;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glfw_context_ver_maj);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glfw_context_ver_min);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr) {
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);


	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);

	//glClearColor(0.f, 0.f, 0.f, 1.0f);
	glClearColor(0.07f, 0.13f, 0.17f, 1.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
