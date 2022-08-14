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
#include "nodeModule/Node.h"
#include "shaderModule/ShaderController.h"
#include "gtc/random.hpp"
#include "core/BoundingVolume.h"
#include "core/ModelLoader.h"
#include "ecsModule/EntityManager.h"

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
	return;

	auto camera = Engine::getInstance()->getCamera();
	auto& projection = camera->getProjectionsMatrix();
    auto view = camera->getComponent<TransformComponent>()->getViewMatrix();

	lightPositions.clear();
	
	for (auto dirLight : lightsObj) {
		dirLight->preDraw();
		batcher->flushAll(false, dirLight->getComponent<TransformComponent>()->getPos(), true);
		dirLight->postDraw();
	}


    // 3. render lights on top of scene
    // --------------------------------
    auto shaderLightBox = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/deferred_light_box.vs", "shaders/deferred_light_box.fs");

    shaderLightBox->use();
    shaderLightBox->setMat4("projection", projection);
    shaderLightBox->setMat4("view", view);

	for (unsigned int i = 0; i < lightPositions.size(); i++) {
		auto model = glm::mat4(1.0f);
		model = glm::translate(model, lightPositions[i]);
		model = glm::scale(model, glm::vec3(0.125f));
		shaderLightBox->setMat4("model", model);
		shaderLightBox->setVec3("lightColor", lightColors[i]);
		batcher->addToDrawList(Utils::cubeVAO, 36, 0, {}, model, false);
	}
	batcher->flushAll(true);

	int  i =0;

	for (auto light : lightsObj) {

		Debug::ComponentsDebug::transformComponentDebug("light" + std::to_string(i), light->getComponent<TransformComponent>());

		auto& scale = light->getComponent<TransformComponent>()->getScale();
		light->getComponent<TransformComponent>()->setScale({0.5f,0.5f,0.5f});
		light->getComponent<TransformComponent>()->reloadTransform();
		
		batcher->addToDrawList(Utils::cubeVAO, 36, 0, {}, light->getComponent<TransformComponent>()->getTransform(), false);
		light->getComponent<TransformComponent>()->setScale(scale);
		light->getComponent<TransformComponent>()->reloadTransform();

		auto lines = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/xyzLines.vs", "shaders/xyzLines.fs");
		lines->use();
		lines->setMat4("PVM",GameEngine::Engine::getInstance()->getCamera()->getProjectionsMatrix() * GameEngine::Engine::getInstance()->getCamera()->getComponent<TransformComponent>()->getViewMatrix() *  light->getComponent<TransformComponent>()->getTransform());
		GameEngine::RenderModule::Utils::renderXYZ(20.f);
		i++;
	}
	
	auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/light.vs", "shaders/light.fs");
    shader->use();
	shader->setMat4("PV", projection * view);
	

	batcher->flushAll(true);



	//if (cascade) {
		//cascade->sunProgress += 0.001f;
		/*auto x = glm::cos(glm::radians(-cascade->sunProgress * 180.f));
		auto y = glm::sin(glm::radians(cascade->sunProgress * 180.f));
		auto z = glm::sin(glm::radians(cascade->sunProgress * 180.f));
		cascade->setLightPosition({x * 80.f, y * 30.f, z * 10.f + 0.001f});*/
	//}
}

void Renderer::postDraw() {
	glfwSwapBuffers(Engine::getInstance()->getMainWindow());
	glfwPollEvents();
}

void Renderer::init() {
	scene = new GameModule::CoreModule::Scene();
	scene->init();

	initGlobalProjection();

	// build and compile shaders
    // -------------------------

	//modelObj = ModelLoader::getInstance()->load("model/scene.gltf");
	//objectPositions.emplace_back(glm::vec3(glm::linearRand(2.3f,4.f), glm::linearRand(2.3f,4.f), glm::linearRand(2.3f,4.f)));

	//modelObj = ModelLoader::getInstance()->load("suzanne/scene.gltf");
	//modelObj = ModelLoader::getInstance()->load("susaLod/untitled.fbx");
	modelObj = ModelLoader::getInstance()->load("sphere.fbx");
	
	auto count = 40;
	for (auto i = 0; i < count; i++) {
		for (auto j = 0; j < count; j++) {
			for (auto k = 1; k < 2 + 1; k++) {
				objectPositions.emplace_back(glm::vec3(i * 10.f /** glm::linearRand(2.3f,4.f)*/, 10 * k/* * glm::linearRand(2.3f,4.f)*/, j * 10.f /** glm::linearRand(2.3f,4.f)*/));
			}
			
		}
	}

	node = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("lel");
	for (auto i = 0u; i < objectPositions.size(); i++) {
		auto& objectPos = objectPositions[i];
		auto backpack = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("backpack" + std::to_string(i));
		backpack->getComponent<TransformComponent>()->setPos(objectPos);
		backpack->getComponent<TransformComponent>()->setScale({0.01f,0.01f,0.01f});
		backpack->getComponent<TransformComponent>()->setRotate({-90.f,0.f,0.f});
		backpack->addComponent<ModelComponent>()->setModel(modelObj);
		backpack->addComponent<RenderComponent>();
		auto lodComp = backpack->addComponent<LodComponent>(eLodType::SCREEN_SPACE);
		lodComp->addLodLevelValue(0.02f);
		lodComp->addLodLevelValue(0.011f);
		lodComp->addLodLevelValue(0.001f);
		node->addElement(backpack);
	}

	auto cube = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("cube");

	node->addElement(cube);

	/*auto cube2 = ecsModule::ECSHandler::entityManagerInstance()->createEntity<NodeModule::Node>("cube2");
	node->addElement(cube2);
	cube2->getComponent<TransformComponent>()->setScale({1.f,10.f,50.f});
	cube2->getComponent<TransformComponent>()->setPos({-10.f,0.f,0.f});
	cube2->addComponent<RenderComponent>();*/

	cube->getComponent<TransformComponent>()->setScale({1.f,1.f,1.f});
	cube->getComponent<TransformComponent>()->setPos({0.f,-1.f,0.f});
	cube->addComponent<RenderComponent>();

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

void Renderer::initGlobalProjection() {
	//todo
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
	glClearDepth(1000.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);


	LogsModule::Logger::LOG_INFO("GLFW initialized");
	return window;
}
