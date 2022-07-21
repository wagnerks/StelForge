#include "Renderer.h"

#include <deque>
#include <iostream>

#include "shader.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "MeshFactory.h"
#include "Engine.h"
#include "logger.h"
#include "Node.h"
#include "ShaderController.h"
#include "componentsModule/TransformComponent.h"
#include "debugModule/ComponentsDebug.h"


constexpr int glfw_context_ver_maj = 3;
constexpr int glfw_context_ver_min = 3;

int SCR_WIDTH = 1920;
int SCR_HEIGHT = 1080;

using namespace GameEngine;
using namespace GameEngine::Render;
using namespace GameEngine::CoreModule;

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	SCR_WIDTH = width;
	SCR_HEIGHT = height;
}

void Renderer::draw() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); 


	Debug::ComponentsDebug::transformComponentDebug(sceneNode->getId(), sceneNode->getComponent<TransformComponent>());

	for (auto node : sceneNode->getAllNodes()) {
		Debug::ComponentsDebug::transformComponentDebug(node->getId(), node->getComponent<TransformComponent>());
	}

	// create transformations
	glm::mat4 projection = glm::perspective(glm::radians(Engine::getInstance()->getCamera().Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

	// camera/view transformation
	glm::mat4 view = Engine::getInstance()->getCamera().GetViewMatrix();

	
	for (auto node : sceneNode->getAllNodes()) {
		
	}
	floor.draw(projection * view);


	auto modelTransform = modelObj->getComponent<TransformComponent>();
	auto model2Transform = modelObj2->getComponent<TransformComponent>();
	
	Debug::ComponentsDebug::transformComponentDebug("model", modelTransform);
	Debug::ComponentsDebug::transformComponentDebug("model2", model2Transform);
	modelTransform->reloadTransform();

	SHADER_CONTROLLER->useDefaultShader();

	SHADER_CONTROLLER->defaultShader->setMat4("PVM", projection * view * modelTransform->getTransform());
	SHADER_CONTROLLER->defaultShader->setMat4("model", modelTransform->getTransform());
	SHADER_CONTROLLER->defaultShader->setMat4("view", view);
	SHADER_CONTROLLER->defaultShader->setMat4("projection", projection);

	SHADER_CONTROLLER->defaultShader->setVec3("lightPos", sceneNode->getElement("light")->getComponent<TransformComponent>()->getPos());
	SHADER_CONTROLLER->defaultShader->setVec3("viewPos", Engine::getInstance()->getCamera().Position);
	SHADER_CONTROLLER->defaultShader->setVec3("objectColor", {1.0f, 0.5f, 0.31f });
	SHADER_CONTROLLER->defaultShader->setVec3("lightColor", {1.0f, 1.0f, 1.0f});

	auto oldPos = modelTransform->getPos();
	for (auto i = 0; i < 6; i++) {
		for (auto j = 0; j < 6; j++) {
			for (auto k = 0; k < 6; k++) {
				modelTransform->setX(oldPos.x + i * 2.f);
				modelTransform->setY(oldPos.y + j * 2.f);
				modelTransform->setZ(oldPos.z + -k * 2.f);
				modelTransform->reloadTransform();
				SHADER_CONTROLLER->defaultShader->setMat4("model", modelTransform->getTransform());
				SHADER_CONTROLLER->defaultShader->setMat4("PVM", projection * view * modelTransform->getTransform());

				modelObj->Draw(SHADER_CONTROLLER->defaultShader);
			}
		}
		
	}

	modelTransform->setPos(oldPos);
	modelObj->Draw(SHADER_CONTROLLER->defaultShader);

	SHADER_CONTROLLER->defaultShader->setMat4("PVM", projection * view * model2Transform->getTransform());
	SHADER_CONTROLLER->defaultShader->setMat4("model", model2Transform->getTransform());
	modelObj2->Draw(SHADER_CONTROLLER->defaultShader);
	
	
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	sceneNode = new Node("scene");
	
	auto childNode = new Node("light");
	auto childNode2 = new Node("child2");


	sceneNode->addElement(childNode2);

	sceneNode->addElement(childNode);


	modelObj = new Model("model/scene.gltf");
	modelObj->getComponent<TransformComponent>()->setScale({0.003f,0.003f,0.003f});

	modelObj2 = new Model("testModel/scene.gltf");
}

void Renderer::terminate() const {
	glfwTerminate();
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

	auto window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
	if (window == nullptr) {
		Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);

	//glDepthMask(GL_FALSE);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glEnable(GL_STENCIL_TEST);

	glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	Logger::LOG_INFO("GLFW initialized");
	return window;
}
