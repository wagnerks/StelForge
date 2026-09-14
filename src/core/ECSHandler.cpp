#include "ECSHandler.h"

#include <fstream>
#include <iostream>

#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "propertiesModule/PropertiesSystem.h"

#include "systemsModule/SystemManager.h"
#include "systemsModule/TasksManager.h"

#include "systemsModule/systems/AABBSystem.h"
#include "systemsModule/systems/ActionSystem.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/ChunksSystem.h"
#include "systemsModule/systems/LODSystem.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/PhysicsSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/SkeletalAnimationSystem.h"
#include "systemsModule/systems/TransformSystem.h"
#include "systemsModule/systems/WorldTimeSystem.h"

namespace SectorsArrayTest
{
	struct Trivial;
}

void ECSHandler::initSystems() {
	//todo separate systems into  drawData preparing and logic update
	/*
	 * draw data preparing functions should not be updated while scene is rendering (or it should have its own ecs registries to apply double buffering (updating data in one buffer, while draw data from other buffer)
	 *
	 * todo also create separate registries for separate data, static data - which is not changes (buildings, woods etc), dynamic data(wooden tree, players etc), shared data (static but needed both for static and dynamic entities)(mesh component for example)
	 */


	//mRegistry.registerArray<TransformComponent, MeshComponent>();
	//mDrawRegistry[0].registerArray<SFE::ComponentsModule::TransformMatComp, MeshComponent, SFE::ComponentsModule::ArmatureBonesComponent>();
	//mDrawRegistry[1].registerArray<SFE::ComponentsModule::TransformMatComp, MeshComponent, SFE::ComponentsModule::ArmatureBonesComponent>();

	mSystemManager.createSystem<SFE::SystemsModule::LODSystem>();
	SFE::Engine::reproTrace("init Transform");
	mSystemManager.createSystem<SFE::SystemsModule::TransformSystem>();
	SFE::Engine::reproTrace("init OcTree");
	mSystemManager.createSystem<SFE::SystemsModule::OcTreeSystem>();
	SFE::Engine::reproTrace("init AABB");
	mSystemManager.createSystem<SFE::SystemsModule::AABBSystem>();
	SFE::Engine::reproTrace("init Camera");
	mSystemManager.createSystem<SFE::SystemsModule::CameraSystem>();
	SFE::Engine::reproTrace("init Chunks");
	mSystemManager.createSystem<SFE::SystemsModule::ChunksSystem>();
	SFE::Engine::reproTrace("init Render");
	mSystemManager.createSystem<SFE::SystemsModule::RenderSystem>();
	SFE::Engine::reproTrace("init Render ok");

	//mSystemManager.createSystem<SFE::SystemsModule::Physics>();
	//mSystemManager.createSystem<SFE::SystemsModule::ActionSystem>();
	//mSystemManager.createSystem<SFE::SystemsModule::WorldTimeSystem>();
	//mSystemManager.createSystem<SFE::SystemsModule::SkeletalAnimationSystem>();


	// Tick systems advance off their own wall-clock threads, so the pose at frame N depends on
	// how fast that run happened to be. A capture has to be reproducible and comparable across
	// backends, so it holds the scene still instead. AnimationComputePass runs its own clock and
	// is frozen separately.
	if (!SFE::Engine::instance()->capturingFrame()) {
		mSystemManager.addTickSystems<SFE::SystemsModule::WorldTimeSystem>(1);
		mSystemManager.addTickSystems<SFE::SystemsModule::SkeletalAnimationSystem>(24);
	}

	mSystemManager.addRootSystems<SFE::SystemsModule::CameraSystem, SFE::SystemsModule::RenderSystem>();

	if (auto chunks = mSystemManager.getSystem<SFE::SystemsModule::ChunksSystem>()) {
		if (auto cam = mSystemManager.getSystem<SFE::SystemsModule::CameraSystem>()) {
			SFE::Engine::reproTrace("init chunks load");
			chunks->updateAsync({ cam->getCurrentCamera() });
			SFE::Engine::reproTrace("init chunks load ok");
		}
	}
}
