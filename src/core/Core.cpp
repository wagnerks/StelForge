#include "Core.h"

#include "ECSHandler.h"
#include "Engine.h"
#include "imgui.h"
#include "InputHandler.h"
#include "multithreading/ThreadPool.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/imguiDecorator.h"
#include "systemsModule/SystemManager.h"
#include "assetsModule/AssetsManager.h"
#include "backends/imgui_impl_opengl3.h"
#include "debugModule/Benchmark.h"
#include "mathModule/Forward.h"
#include "renderModule/TextRenderer.h"

namespace SFE::CoreModule {
	void Core::update(float dt) {
		FUNCTION_BENCHMARK;

		ECSHandler::systemManager().update(dt);
		ThreadPool::instance()->syncUpdate();
	}

	void Core::init() {
		ECSHandler::instance()->initSystems();
		CoreModule::InputHandler::init(Engine::instance()->getWindow());
	}

	Core::~Core() {
		SFE::ShaderModule::ShaderController::terminate();
		AssetsModule::TextureHandler::terminate();
		AssetsModule::ModelLoader::terminate();

		ThreadPool::terminate();
		ECSHandler::terminate();

		AssetsModule::AssetsManager::terminate();
	}
}