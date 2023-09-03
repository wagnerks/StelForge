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
#include "shaderModule/ShaderController.h"

namespace Engine {
	void UnnamedEngine::init() {
		window = RenderModule::Renderer::initGLFW();
		if (!window) {
			alive = false;
			return;
		}

		core = new CoreModule::Core();
		core->init();
		render = new RenderModule::Renderer();
		camera = ecsModule::ECSHandler::entityManagerInstance()->createEntity<Camera>(::Engine::ProjectionModule::Projection{45.f, static_cast<float>(RenderModule::Renderer::SCR_WIDTH) / static_cast<float>(RenderModule::Renderer::SCR_HEIGHT), 0.1f, 5000.f});

		render->init();
		ecsModule::ECSHandler::instance()->initSystems();

		CoreModule::InputHandler::init();

		Debug::ImGuiDecorator::init(getMainWindow());
	}

	void UnnamedEngine::update() {
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

	float UnnamedEngine::getDeltaTime() const {
		return deltaTime;
	}

	int UnnamedEngine::getFPS() const {
		return fps;
	}

	void UnnamedEngine::updateDelta() {
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

	bool UnnamedEngine::isAlive() {
		return alive;
	}

	bool UnnamedEngine::checkNeedClose() {
		return !glfwWindowShouldClose(window);
	}

	GLFWwindow* UnnamedEngine::getMainWindow() {
		return window;
	}

	Camera* UnnamedEngine::getCamera() {
		return camera;
	}

	RenderModule::Renderer* UnnamedEngine::getRenderer() const {
		return render;
	}

	UnnamedEngine::UnnamedEngine() {

	}

	UnnamedEngine::~UnnamedEngine() {
		delete core;
		glfwDestroyWindow(getMainWindow());
		delete render;
		//delete camera;


		Engine::ShaderModule::ShaderController::terminate();
		RenderModule::TextureHandler::terminate();

		ecsModule::ECSHandler::terminate();
	}
}
