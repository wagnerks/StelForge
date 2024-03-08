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
	mRegistry.initCustomComponentsContainer<TransformComponent, MeshComponent>();

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
		auto model = AssetsModule::ModelLoader::instance()->load(path);
		for (auto i = 0; i < 2; i++) {
			for (auto j = 0; j < 2; j++) {
				if (!SFE::Engine::instance()->isAlive()) {
					return;
				}
				auto entity = ECSHandler::registry().takeEntity();

				ECSHandler::registry().addComponent<SFE::ComponentsModule::AABBComponent>(entity);
				ECSHandler::registry().addComponent<OcTreeComponent>(entity);
				auto modelComp = ECSHandler::registry().addComponent<ModelComponent>(entity, entity.getID());

				modelComp->mPath = path;
				modelComp->boneMatrices = model->getDefaultBoneMatrices();
				modelComp->armature = model->getArmature();
				modelComp->setModel(model->getLODs());

				auto transformComp = ECSHandler::registry().addComponent<TransformComponent>(entity, entity.getID());
				transformComp->setPos({ i * 40.f, 0.f, j * 20.f });
				transformComp->setScale({ 0.1f });
				ECSHandler::registry().addComponent<IsDrawableComponent>(entity);

				if (modelComp && !modelComp->getModel().meshes.empty()) {
					auto meshComp = ECSHandler::registry().addComponent<MeshComponent>(entity);
		
					meshComp->meshTree.fillTree<SFE::MeshObject3D>(model->getMeshTree(), [](const SFE::MeshObject3D& meshObj) {
						return MeshComponent::MeshData { SFE::MeshVaoRegistry::instance()->get(const_cast<SFE::Mesh3D*>(&meshObj.mesh)).vao.getID(), static_cast<int>(meshObj.mesh.vertices.size()), static_cast<int>(meshObj.mesh.indices.size()) };
					});

					auto materialComp = ECSHandler::registry().addComponent<MaterialComponent>(entity);
					meshComp->meshModel = model;
					for (auto& mat : modelComp->getModel().meshes[0]->material.materialTextures) {
						materialComp->materials.addMaterial({ mat.second.uniformSlot, mat.second.texture->mId, mat.second.texture->mType });
					}

					auto armatureComp = ECSHandler::registry().addComponent<SFE::ComponentsModule::ArmatureComponent>(entity);
					armatureComp->armature = modelComp->armature;
					std::ranges::copy(modelComp->boneMatrices, armatureComp->boneMatrices);
				}

				auto animComp = ECSHandler::registry().addComponent<SFE::ComponentsModule::AnimationComponent>(entity);
				animComp->mCurrentAnimation = &model->getAnimations()[0];
				animComp->mCurrentTime = static_cast<float>(i);
				auto oclC = ECSHandler::registry().addComponent<SFE::ComponentsModule::OcclusionComponent>(entity);
				oclC->query = new SFE::GLW::Query<SFE::GLW::QueryType::SAMPLES_PASSED>();
				oclC->query->generate();
			}
		}
		
	});
}
