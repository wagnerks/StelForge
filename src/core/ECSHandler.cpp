#include "ECSHandler.h"

#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/OcclusionComponent.h"
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
#include "systemsModule/systems/SkeletalAnimationSystem.h"
#include "systemsModule/systems/TransformSystem.h"
#include "systemsModule/systems/WorldTimeSystem.h"

void ECSHandler::initSystems() {
	//todo separate systems into  drawData preparing and logic update
	/*
	 * draw data preparing functions should not be updated while scene is rendering (or it should have its own ecs registries to apply double buffering (updating data in one buffer, while draw data from other buffer)
	 *
	 * todo also create separate registries for separate data, static data - which is not changes (buildings, woods etc), dynamic data(wooden tree, players etc), shared data (static but needed both for static and dynamic entities)(mesh component for example)
	 */

	//mRegistry.initCustomComponentsContainer<TransformComponent, MeshComponent>();
	//mDrawRegistry[0].initCustomComponentsContainer<SFE::ComponentsModule::TransformMatComp, MeshComponent, SFE::ComponentsModule::ArmatureBonesComponent>();
	//mDrawRegistry[1].initCustomComponentsContainer<SFE::ComponentsModule::TransformMatComp, MeshComponent, SFE::ComponentsModule::ArmatureBonesComponent>();


	mSystemManager.createSystem<SFE::SystemsModule::TransformSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::ActionSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::OcTreeSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::AABBSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::ChunksSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::WorldTimeSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::SkeletalAnimationSystem>();

	mSystemManager.createSystem<SFE::SystemsModule::CameraSystem>();
	mSystemManager.createSystem<SFE::SystemsModule::RenderSystem>();

	mSystemManager.createSystem<SFE::SystemsModule::Physics>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::Physics>(1 / 60.f);

	mSystemManager.createSystem<SFE::SystemsModule::LODSystem>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::LODSystem>(60.f);

	mSystemManager.createSystem<SFE::SystemsModule::ShaderSystem>();
	mSystemManager.setUpdateInterval<SFE::SystemsModule::ShaderSystem>(1 / 60.f);

	mSystemManager.addTickSystems<SFE::SystemsModule::Physics, SFE::SystemsModule::ActionSystem, SFE::SystemsModule::WorldTimeSystem, SFE::SystemsModule::SkeletalAnimationSystem>();
	mSystemManager.addRootSystems<SFE::SystemsModule::CameraSystem, SFE::SystemsModule::RenderSystem, SFE::SystemsModule::ShaderSystem>();

	mSystemManager.setSystemDependencies<SFE::SystemsModule::TransformSystem, SFE::SystemsModule::AABBSystem>();
	mSystemManager.setSystemDependencies<SFE::SystemsModule::AABBSystem, SFE::SystemsModule::OcTreeSystem>();
	mSystemManager.setSystemDependencies<SFE::SystemsModule::CameraSystem, SFE::SystemsModule::ChunksSystem>();

	mSystemManager.startTickSystems();

	SFE::ThreadPool::instance()->addTask([]() {
		SFE::PropertiesModule::PropertiesSystem::loadScene("shadowsTest.json");
		//SFE::PropertiesModule::PropertiesSystem::loadScene("stressTest.json");
		
		auto path = "models/vampire.fbx";
		//auto path = "models/cube.fbx";
		auto model = AssetsModule::ModelLoader::instance()->load(path);
		for (auto i = 0; i < 20; i++) {
			for (auto j = 0; j < 20; j++) {
				for (auto k = 0; k < 1; k++) {
					if (!SFE::Engine::instance()->isAlive()) {
						return;
					}
					auto entity = ECSHandler::registry().takeEntity();

					ECSHandler::addComponent<SFE::ComponentsModule::AABBComponent>(entity);
					ECSHandler::addComponent<OcTreeComponent>(entity);
					auto modelComp = ECSHandler::addComponent<ModelComponent>(entity, entity);

					modelComp->mPath = path;
					modelComp->boneMatrices = model->getDefaultBoneMatrices();
					modelComp->armature = model->getArmature();
					modelComp->setModel(model->getLODs());

					auto transformComp = ECSHandler::addComponent<TransformComponent>(entity, entity);
					transformComp->setPos({ i * 40.f, k*40.f, j * 40.f });
					transformComp->setScale({ 0.1f });
					ECSHandler::addComponent<IsDrawableComponent>(entity);

					if (modelComp && !modelComp->getModel().meshes.empty()) {
						auto meshComp = ECSHandler::addComponent<MeshComponent>(entity);
						meshComp->meshGraph.fill<SFE::MeshObject3D>(model->getMeshTree(), [](const SFE::MeshObject3D& meshObj) {
							return MeshComponent::MeshData {
								SFE::MeshVaoRegistry::instance()->get(const_cast<SFE::Mesh3D*>(&meshObj.mesh)).vao.getID(),
								static_cast<int>(meshObj.mesh.vertices.size()),
								static_cast<int>(meshObj.mesh.indices.size())
							};
						});

						if (auto renderSys = ECSHandler::systemManager().getSystem<SFE::SystemsModule::RenderSystem>()) {
							renderSys->markDirty<MeshComponent>(entity);
						}


						if (modelComp->getModel().meshes[0]->material.materialTextures.size()) {
							auto materialComp = ECSHandler::addComponent<MaterialComponent>(entity);
							meshComp->meshModel = model;
							for (auto& mat : modelComp->getModel().meshes[0]->material.materialTextures) {
								materialComp->materials.addMaterial({ mat.second.uniformSlot, mat.second.texture->mId, mat.second.texture->mType });
							}
							if (auto renderSys = ECSHandler::systemManager().getSystem<SFE::SystemsModule::RenderSystem>()) {
								renderSys->markDirty<MaterialComponent>(entity);
							}
						}

						if (modelComp->armature.bones.size()) {
							auto armatureComp = ECSHandler::addComponent<SFE::ComponentsModule::ArmatureComponent>(entity);
							armatureComp->armature = modelComp->armature;

							auto armatureBonesComp = ECSHandler::registry().addComponent<SFE::ComponentsModule::ArmatureBonesComponent>(entity);
							std::ranges::copy(modelComp->boneMatrices, armatureBonesComp->boneMatrices.begin());
							if (auto renderSys = ECSHandler::systemManager().getSystem<SFE::SystemsModule::RenderSystem>()) {
								renderSys->markDirty<SFE::ComponentsModule::ArmatureComponent>(entity);
							}
						}
					}

					if (model->getAnimations().size()) {
						auto animComp = ECSHandler::addComponent<SFE::ComponentsModule::AnimationComponent>(entity);
						animComp->mCurrentAnimation = &model->getAnimations()[0];
						animComp->mCurrentTime = static_cast<float>(i);
					}
					



					/*auto oclC = ECSHandler::addComponent<SFE::ComponentsModule::OcclusionComponent>(entity);
					oclC->query = new SFE::GLW::Query<SFE::GLW::QueryType::SAMPLES_PASSED>();
					oclC->query->generate();
					auto center = SFE::Math::Vec3{ i * 40.f, k * 40.f, j * 40.f };
					auto min = center - SFE::Math::Vec3{ 10.f, 10.f, 10.f };
					auto max = center + SFE::Math::Vec3{ 10.f, 10.f, 10.f};

					auto minB = center - SFE::Math::Vec3{ 9.f, 9.f, 9.f };
					auto maxB = center + SFE::Math::Vec3{ 9.f, 9.f, 9.f };*/
					//oclC->occludeeAABB.push_back(SFE::FrustumModule::AABB(min, max));
					//if (i == 0 && j == 0) {
					//oclC->occluderAABB.push_back(SFE::FrustumModule::AABB(minB, maxB));
					//}
				}
			}
		}
	});
}
