#include "Core.h"

#include "ECSHandler.h"
#include "Engine.h"
#include "imgui.h"
#include "InputHandler.h"
#include "multithreading/ThreadPool.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/ComponentsDebug.h"
#include "debugModule/imguiDecorator.h"
#include "systemsModule/SystemManager.h"
#include "assetsModule/AssetsManager.h"
#include "backends/imgui_impl_opengl3.h"
#include "debugModule/Benchmark.h"
#include "mathModule/Forward.h"


using namespace SFE;
using namespace SFE::CoreModule;

void Core::update(float dt) {
	FUNCTION_BENCHMARK;
	
	mDecorator.preDraw();
	ECSHandler::systemManager().update(dt);
	mDebugMenu.draw();
	ThreadPool::instance()->syncUpdate();
	Debug::ComponentsDebug::instance()->entitiesDebug();
	Debug::BenchmarkGUI::instance()->onGui();

	mDecorator.draw();
	Render::Renderer::instance()->swapBuffer();
}

void Core::init() {
	ECSHandler::instance()->initSystems();
	CoreModule::InputHandler::init();
	Render::Renderer::instance();
	
	mDecorator.init(SFE::Engine::instance()->getMainWindow());
}

Core::~Core() {
	SFE::ShaderModule::ShaderController::terminate();
	AssetsModule::TextureHandler::terminate();
	AssetsModule::ModelLoader::terminate();
	
	
	ThreadPool::terminate();
	ECSHandler::terminate();

	Debug::ComponentsDebug::terminate();
;	AssetsModule::AssetsManager::terminate();
	mDecorator.destroyContext();

	Render::Renderer::terminate();
}