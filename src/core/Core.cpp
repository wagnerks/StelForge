#include "Core.h"

#include "ECSHandler.h"
#include "InputHandler.h"
#include "assetsModule/modelModule/ModelLoader.h"
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

	ECSHandler::entityManagerInstance()->destroyEntities();
	ECSHandler::systemManagerInstance()->update(dt);

	mDebugMenu.draw();

	RenderModule::Renderer::instance()->draw();

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

}

Core::Core() {}