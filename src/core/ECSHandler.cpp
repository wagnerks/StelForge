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

	mSystemManager.createSystem<SFE::SystemsModule::TransformSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::ActionSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::OcTreeSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::AABBSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::ChunksSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::WorldTimeSystem>();

	mSystemManager.createSystem<SFE::SystemsModule::CameraSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::RenderSystem>(SFE::RenderModule::Renderer::instance());

	mSystemManager.createSystem<SFE::SystemsModule::Physics>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::Physics>(1 / 60.f);

	mSystemManager.createSystem<SFE::SystemsModule::LODSystem>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::LODSystem>(60.f);

	mSystemManager.createSystem<SFE::SystemsModule::ShaderSystem>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::ShaderSystem>(1 / 60.f);


	mSystemManager.addRootSystems<SFE::SystemsModule::CameraSystem, SFE::SystemsModule::RenderSystem, SFE::SystemsModule::ShaderSystem, SFE::SystemsModule::Physics, SFE::SystemsModule::WorldTimeSystem, SFE::SystemsModule::ActionSystem>();

	mSystemManager.setSystemDependencies<SFE::SystemsModule::TransformSystem, SFE::SystemsModule::AABBSystem>();
	mSystemManager.setSystemDependencies<SFE::SystemsModule::AABBSystem, SFE::SystemsModule::OcTreeSystem>();
	mSystemManager.setSystemDependencies<SFE::SystemsModule::CameraSystem, SFE::SystemsModule::ChunksSystem>();

	SFE::ThreadPool::instance()->addTask([]() {
		SFE::PropertiesModule::PropertiesSystem::loadScene("shadowsTest.json");
		//SFE::PropertiesModule::PropertiesSystem::loadScene("stressTest.json");
	});
}
