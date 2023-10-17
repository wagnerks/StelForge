#include "Core.h"

#include "ECSHandler.h"
#include "InputHandler.h"
#include "ThreadPool.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/ComponentsDebug.h"
#include "debugModule/imguiDecorator.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "..\ecss\Registry.h"
#include "systemsModule/SystemManager.h"
#include "assetsModule/AssetsManager.h"

using namespace Engine;
using namespace Engine::CoreModule;

void Core::update(float dt) {
	Debug::ImGuiDecorator::preDraw();

	ECSHandler::systemManager()->update(dt);

	mDebugMenu.draw();

	static auto threadID = std::this_thread::get_id();
	if (threadID != std::this_thread::get_id()) {
		int d = 0;
	}
	RenderModule::Renderer::instance()->draw();

	ThreadPool::instance()->synchroUpdate();

	Debug::ComponentsDebug::entitiesDebug();

	Debug::ImGuiDecorator::draw();

	RenderModule::Renderer::instance()->postDraw();

}

void Core::init() {
	ECSHandler::instance()->initSystems();
	CoreModule::InputHandler::instance();
	RenderModule::Renderer::instance();
}

Core::~Core() {
	Engine::ShaderModule::ShaderController::terminate();
	AssetsModule::TextureHandler::terminate();
	AssetsModule::ModelLoader::terminate();
	RenderModule::Renderer::terminate();
	ECSHandler::terminate();
	CoreModule::InputHandler::terminate();
	ThreadPool::terminate();

	AssetsModule::AssetsManager::terminate();
}

Core::Core() {}