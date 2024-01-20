#include "ECSHandler.h"

#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"
#include "propertiesModule/PropertiesSystem.h"

#include "systemsModule/SystemManager.h"

#include "systemsModule/systems/AABBSystem.h"
#include "systemsModule/systems/ActionSystem.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/ChunksSystem.h"
#include "systemsModule/systems/LODSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/PhysicsSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ShaderSystem.h"
#include "systemsModule/systems/TransformSystem.h"
#include "systemsModule/systems/WorldTimeSystem.h"

void ECSHandler::initSystems() {
	mRegistry.initCustomComponentsContainer<TransformComponent, ModelComponent>();

	mSystemManager.createSystem<Engine::SystemsModule::TransformSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::ActionSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::OcTreeSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::AABBSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::ChunksSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::WorldTimeSystem>();

	mSystemManager.createSystem<Engine::SystemsModule::CameraSystem>();
	mSystemManager.createSystem<Engine::SystemsModule::RenderSystem>(Engine::RenderModule::Renderer::instance());

	mSystemManager.createSystem<Engine::SystemsModule::Physics>();
	mSystemManager.setUpdateInterval<Engine::SystemsModule::Physics>(1 / 60.f);

	mSystemManager.createSystem<Engine::SystemsModule::LODSystem>();
	mSystemManager.setUpdateInterval<Engine::SystemsModule::LODSystem>(60.f);

	mSystemManager.createSystem<Engine::SystemsModule::ShaderSystem>();
	mSystemManager.setUpdateInterval<Engine::SystemsModule::ShaderSystem>(1 / 60.f);


	mSystemManager.addRootSystems<Engine::SystemsModule::CameraSystem, Engine::SystemsModule::RenderSystem, Engine::SystemsModule::ShaderSystem, Engine::SystemsModule::Physics, Engine::SystemsModule::WorldTimeSystem, Engine::SystemsModule::ActionSystem>();

	mSystemManager.setSystemDependencies<Engine::SystemsModule::TransformSystem, Engine::SystemsModule::AABBSystem>();
	mSystemManager.setSystemDependencies<Engine::SystemsModule::AABBSystem, Engine::SystemsModule::OcTreeSystem>();
	mSystemManager.setSystemDependencies<Engine::SystemsModule::CameraSystem, Engine::SystemsModule::ChunksSystem>();

	Engine::ThreadPool::instance()->addTask([]() {
		Engine::PropertiesModule::PropertiesSystem::loadScene("shadowsTest.json");
		//Engine::PropertiesModule::PropertiesSystem::loadScene("stressTest.json");
	});
}
