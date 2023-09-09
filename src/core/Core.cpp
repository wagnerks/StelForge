#include "Core.h"

#include "InputHandler.h"
#include "ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/ComponentsDebug.h"
#include "debugModule/imguiDecorator.h"
#include "ecsModule/EntityManager.h"
#include "ecsModule/SystemManager.h"
#include "assetsModule/shaderModule/ShaderController.h"

using namespace Engine;
using namespace Engine::CoreModule;

void Core::update(float dt) {
	Debug::ImGuiDecorator::preDraw();

	ecsModule::ECSHandler::entityManagerInstance()->destroyEntities();
	ecsModule::ECSHandler::systemManagerInstance()->update(dt);

	mDebugMenu.draw();

	RenderModule::Renderer::instance()->draw();

	Debug::ComponentsDebug::entitiesDebug();

	Debug::ImGuiDecorator::draw();

	RenderModule::Renderer::instance()->postDraw();

}

void Core::init() {
	ecsModule::ECSHandler::instance()->initSystems();
	CoreModule::InputHandler::instance();
	RenderModule::Renderer::instance();
}

Core::~Core() {
	Engine::ShaderModule::ShaderController::terminate();
	AssetsModule::TextureHandler::terminate();
	AssetsModule::ModelLoader::terminate();
	RenderModule::Renderer::terminate();
	ecsModule::ECSHandler::terminate();
	CoreModule::InputHandler::terminate();
	
}

Core::Core() {}