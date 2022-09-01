#include "Renderer.h"

#include <deque>
#include <functional>
#include <iostream>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "imgui.h"
#include "TextureHandler.h"
#include "Utils.h"
#include "componentsModule/LodComponent.h"
#include "componentsModule/RenderComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Camera.h"
#include "core/Engine.h"
#include "debugModule/ComponentsDebug.h"
#include "logsModule/logger.h"
#include "modelModule/MeshFactory.h"
#include "modelModule/Model.h"

#include "shaderModule/ShaderController.h"
#include "gtc/random.hpp"
#include "core/BoundingVolume.h"
#include "core/ModelLoader.h"
#include "ecsModule/EntityManager.h"
#include "entitiesModule/ModelEntity.h"

constexpr int GLFW_CONTEXT_VER_MAJ = 4;
constexpr int GLFW_CONTEXT_VER_MIN = 6;


using namespace GameEngine;
using namespace GameEngine::RenderModule;
using namespace GameEngine::CoreModule;


void Renderer::draw() {
	RenderModule::Renderer::drawCallsCount = 0;
	RenderModule::Renderer::drawVerticesCount = 0;
	Utils::initCubeVAO();
	scene->updateScene(0.f);
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	scene = new GameModule::CoreModule::Scene();
	scene->init();

	auto cubeModel = ModelLoader::getInstance()->load("model/cube.fbx");
	

	node = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Model>();

	auto cube = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Model>();
	cube->setStringId("floor");
	cube->getComponent<TransformComponent>()->setScale({50.f,0.01f,50.f});
	cube->getComponent<TransformComponent>()->setPos({0.f,-1.f,0.f});
	cube->getComponent<ModelComponent>()->setModel(cubeModel);

	node->addElement(cube);

	auto cube2 = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Model>();
	cube2->setStringId("wall");
	cube2->getComponent<TransformComponent>()->setScale({0.01f,0.1f,5.f});
	cube2->getComponent<TransformComponent>()->setPos({-10.f,0.f,0.f});
	cube2->getComponent<ModelComponent>()->setModel(cubeModel);
	
	node->addElement(cube2);

	auto sponza = ModelLoader::getInstance()->load("models/sponza/scene.gltf");
	auto sponzaModel = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Model>();
	sponzaModel->setStringId("sponza");
	sponzaModel->getComponent<TransformComponent>()->setScale({1.f,1.f,1.f});
	sponzaModel->getComponent<TransformComponent>()->setPos({-25.f,25.f,0.f});
	sponzaModel->getComponent<TransformComponent>()->setRotate({0.f,0.f,180.f});
	sponzaModel->getComponent<ModelComponent>()->setModel(sponza);
	node->addElement(sponzaModel);

	auto count = 3;
	for (auto i = 0; i < count; i++) {
		for (auto j = 0; j < count; j++) {
			for (auto k = 1; k < count + 1; k++) {
				auto trainNode = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Model>();
				trainNode->getComponent<ModelComponent>()->setModel(cubeModel);
				trainNode->getComponent<TransformComponent>()->setRotateX(0.f);
				trainNode->getComponent<TransformComponent>()->setScale({0.01f,0.01f,0.01f});

				trainNode->getComponent<TransformComponent>()->setPos(glm::vec3(i * 10.f * glm::linearRand(2.3f,4.f), k * 10.f * glm::linearRand(2.3f,4.f), j * 10.f * glm::linearRand(2.3f,4.f)));
				node->addElement(trainNode);
			}
			
		}
	}
	
	// lighting info
    // -------------
    const unsigned int NR_LIGHTS = 0;
    
    srand(13);
	for (unsigned int i = 0; i < 1600; i++) {
		// calculate slightly random offsets

		// also calculate random color
		float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
		lightColors.push_back(glm::vec3(rColor, gColor, bColor));
	}
	
	// shader configuration
    // --------------------
	
	batcher = new Batcher();

	//lightsObj.push_back(new LightsModule::DirectionalLight(1024,1024));
}

void Renderer::terminate() const {
	glfwTerminate();
}

void Renderer::drawCall() {
	
}

void Renderer::drawArrays(GLenum mode, GLsizei size, GLint first) {
	glDrawArrays(mode, first, size);
	drawCallsCount++;
	drawVerticesCount += size;
}

void Renderer::drawElements(GLenum mode, GLsizei size, GLenum type, const void* place) {
	glDrawElements(mode, size, type, place);
	drawCallsCount++;
	drawVerticesCount += size;
}

void Renderer::drawElementsInstanced(GLenum mode, GLsizei size, GLenum type, GLsizei instancesCount, const void* place) {
	glDrawElementsInstanced(mode, size, type, place, instancesCount);
	drawCallsCount++;
	drawVerticesCount += size * instancesCount;
}

void Renderer::drawArraysInstancing(GLenum mode, GLsizei size, GLsizei instancesCount, GLint first) {
	glDrawArraysInstanced(mode, first, size, instancesCount);
	drawCallsCount++;
	drawVerticesCount += size * instancesCount;
}

GLFWwindow* Renderer::initGLFW() {
	if (GLFWInited) {
		assert(false && "GLFW Already inited");
		return nullptr;
	}
	GLFWInited = true;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GLFW_CONTEXT_VER_MAJ);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GLFW_CONTEXT_VER_MIN);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	auto window = glfwCreateWindow(Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, "GameEngine", nullptr, nullptr);
	if (window == nullptr) {
		LogsModule::Logger::LOG_ERROR("Failed to create GLFW window");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		glViewport(0, 0, width, height);
		Renderer::SCR_WIDTH = width;
		Renderer::SCR_HEIGHT = height;
	});

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogsModule::Logger::LOG_ERROR("Failed to initialize GLAD");
		glfwTerminate();
		glfwDestroyWindow(window);
		return nullptr;
	}

	glfwSwapInterval(0);


	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	//glDepthFunc(GL_LESS);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glClearDepth(50000.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
