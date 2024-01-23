#include "Core.h"

#include "ECSHandler.h"
#include "InputHandler.h"
#include "ThreadPool.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "debugModule/ComponentsDebug.h"
#include "debugModule/imguiDecorator.h"
#include "systemsModule/SystemManager.h"
#include "assetsModule/AssetsManager.h"
#include "debugModule/Benchmark.h"
#include "mathModule/Forward.h"


using namespace SFE;
using namespace SFE::CoreModule;

void Core::update(float dt) {
	FUNCTION_BENCHMARK;
	
	Debug::ImGuiDecorator::preDraw();
	ECSHandler::systemManager().update(dt);
	mDebugMenu.draw();
	RenderModule::Renderer::instance()->draw();
	ThreadPool::instance()->syncUpdate();
	Debug::ComponentsDebug::instance()->entitiesDebug();
	Debug::BenchmarkGUI::instance()->onGui();

	Debug::ImGuiDecorator::draw();
	RenderModule::Renderer::instance()->postDraw();
}

void Core::init() {
	AssetsModule::ModelLoader::instance()->load("models/cube.fbx");
	ECSHandler::instance()->initSystems();
	CoreModule::InputHandler::init();
	RenderModule::Renderer::instance();

	/*{
		Math::Matrix<float, 2, 2> A {1, 4, 2, 4};
		Math::Matrix<float, 2, 2> B {2, 6, 3, 7};
		Math::Matrix<float, 2, 2> expectedC = { 14, 32, 17,40 };
		
		Math::Vec3 A1 = { 1,2,3 };
		Math::Vec3 A2 = { 5,6,7 };
		Math::Vec2 KEK = { 1.f,2.f };
		Math::Vec2 expected = { 5,12 };
		
		auto A3 = A1 * A2;
		A2 += A1;
		A2 = -A2;

		auto C = A * B;
		auto D = A * KEK;
		assert(C == expectedC);
		assert(D == expected);
	}*/
}

Core::~Core() {
	SFE::ShaderModule::ShaderController::terminate();
	AssetsModule::TextureHandler::terminate();
	AssetsModule::ModelLoader::terminate();
	RenderModule::Renderer::terminate();
	
	ThreadPool::terminate();
	ECSHandler::terminate();

	Debug::ComponentsDebug::terminate();
;	AssetsModule::AssetsManager::terminate();
}

Core::Core() {}