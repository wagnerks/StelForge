#include "Engine.h"

#include "debugModule/imguiDecorator.h"
#include <chrono>
#include <thread>

#include "Camera.h"
#include "Core.h"
#include "InputHandler.h"
#include "debugModule/ComponentsDebug.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"
#include "logsModule/logger.h"

using namespace GameEngine;

Engine* Engine::getInstance() {
	if (!instance) {
		instance = new Engine();
	}

	return instance;
}

void Engine::terminate() {
	LogsModule::Logger::LOG_INFO("Engine::Terminating");

	delete instance;
	instance = nullptr;
}

void Engine::init() {
	window = RenderModule::Renderer::initGLFW();
	if (!window) {
		alive = false;
		return;
	}

	

	core = new CoreModule::Core();
	core->init();
	render = new RenderModule::Renderer();
	camera = ecsModule::ECSHandler::entityManagerInstance()->createEntity<Camera>(GameEngine::ProjectionModule::PerspectiveProjection{45.f, static_cast<float>(RenderModule::Renderer::SCR_WIDTH) / static_cast<float>(RenderModule::Renderer::SCR_HEIGHT), 0.1f, 5000.f});

	render->init();
	ecsModule::ECSHandler::getInstance()->initSystems();

	CoreModule::InputHandler::init();
	
	Debug::ImGuiDecorator::init(getMainWindow());
}

void Engine::update() {
	if (!alive) {
		return;
	}

	CoreModule::InputHandler::processInput(getMainWindow());

	updateDelta();

	core->update(deltaTime);

	Debug::ImGuiDecorator::preDraw();

	ecsModule::ECSHandler::entityManagerInstance()->destroyEntities();
	ecsModule::ECSHandler::systemManagerInstance()->update(deltaTime);

	debugMenu.draw();

	render->draw();

	Debug::ComponentsDebug::entitiesDebug();
	Debug::ImGuiDecorator::draw();

	render->postDraw();

	alive = checkNeedClose();
}

float Engine::getDeltaTime() const {
	return deltaTime;
}

int Engine::getFPS() const {
	return fps;
}

void Engine::updateDelta() {
	const auto currentFrame = static_cast<float>(glfwGetTime());
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	framesPerSecond++;
	fpsTimer += deltaTime;
	if (fpsTimer > 1.f) {
		fps = framesPerSecond;
		framesPerSecond = 0;
		fpsTimer = 0.f;
	}
}

bool Engine::isAlive() {
	return alive;
}

bool Engine::checkNeedClose() {
	return !glfwWindowShouldClose(window);
}

GLFWwindow* Engine::getMainWindow() {
	return window;
}

Camera* Engine::getCamera() {
	return camera;
}

RenderModule::Renderer* Engine::getRenderer() const {
	return render;
}

Engine::Engine() {
	
}

Engine::~Engine() {
	delete core;
	glfwDestroyWindow(getMainWindow());
	delete render;
	//delete camera;
	ecsModule::ECSHandler::terminate();
}
