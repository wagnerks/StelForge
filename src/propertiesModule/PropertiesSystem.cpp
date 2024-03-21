#include "propertiesModule/PropertiesSystem.h"
#include "core/FileSystem.h"

#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/ECSHandler.h"
#include "multithreading/ThreadPool.h"

namespace SFE::PropertiesModule {
	ecss::EntityId PropertiesSystem::loadScene(std::string_view path) {
		if (!FileSystem::isFileExists(path)) {
			return {};
		}

		auto scene = ECSHandler::registry().takeEntity();

		fillTree(scene, FileSystem::readJson(path));

		return scene;
	}

	void PropertiesSystem::applyProperties(ecss::EntityId entity, const Json::Value& properties) {
		if (entity == ecss::INVALID_ID) {
			return;
		}
		if (!properties.isMember("Properties")) {
			return;
		}

		deserializeProperty<ModelComponent>(entity, properties["Properties"]);
		deserializeProperty<TransformComponent>(entity, properties["Properties"]);
	}

	void PropertiesSystem::fillTree(ecss::EntityId entity, const Json::Value& properties) {
		if (!properties.isObject()) {
			return;
		}

		if (properties.isMember("id")) {
			auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(entity);
			debugData->stringId = properties["id"].asString();
		}

		ECSHandler::registry().addComponent<ComponentsModule::AABBComponent>(entity);
		ECSHandler::registry().addComponent<OcTreeComponent>(entity);
		applyProperties(entity, properties);
		ECSHandler::registry().addComponent<IsDrawableComponent>(entity);
		auto modelComp = ECSHandler::registry().getComponent<ModelComponent>(entity);
		if (modelComp && !modelComp->getModel().meshes.empty()) {
			auto meshComp = ECSHandler::registry().addComponent<MeshComponent>(entity);
			meshComp->meshGraph.root().value = { MeshVaoRegistry::instance()->get(&modelComp->getModel().meshes[0]->mesh).vao.getID(), static_cast<int>(modelComp->getModel().meshes[0]->mesh.vertices.size()), static_cast<int>(modelComp->getModel().meshes[0]->mesh.indices.size()) };

			auto materialComp = ECSHandler::registry().addComponent<MaterialComponent>(entity);
			for (auto& mat : modelComp->getModel().meshes[0]->material.materialTextures) {
				materialComp->materials.addMaterial({ mat.second.uniformSlot, mat.second.texture->mId, mat.second.texture->mType});
			}

			auto armatureComp = ECSHandler::registry().addComponent<ComponentsModule::ArmatureComponent>(entity);
			auto armatureBonesComp = ECSHandler::registry().addComponent<ComponentsModule::ArmatureBonesComponent>(entity);
			
			armatureComp->armature = modelComp->armature;

			std::ranges::copy(modelComp->boneMatrices, armatureBonesComp->boneMatrices.begin());
		}
		
		auto treeComp = ECSHandler::registry().addComponent<TreeComponent>(entity, entity);

		if (properties.isMember("Children") && properties["Children"].isArray()) {
			for (auto element : properties["Children"]) {
				auto child = ECSHandler::registry().takeEntity();				
				fillTree(child, element);
				treeComp->addChildEntity(child);
			}
		}
	}

	Json::Value PropertiesSystem::serializeEntity(ecss::EntityId entity) {
		Json::Value result = Json::objectValue;
		if (entity == ecss::INVALID_ID) {
			return result;
		}

		if (auto debugData = ECSHandler::registry().getComponent<DebugDataComponent>(entity)) {
			result["id"] = debugData->stringId;
		}
		

		serializeProperty<ComponentsModule::TransformComponent>(entity, result["Properties"]);
		serializeProperty<ModelComponent>(entity, result["Properties"]);
		serializeProperty<CascadeShadowComponent>(entity, result["Properties"]);

		auto treeComp = ECSHandler::registry().getComponent<ComponentsModule::TreeComponent>(entity);
		auto children = treeComp ? treeComp->getChildren() : std::vector<ecss::SectorId>();
		if (!children.empty()) {
			result["Children"] = Json::arrayValue;
			auto& childrenJson = result["Children"];
			for (auto node : children) {
				childrenJson.append(serializeEntity(node));
			}
		}

		return result;
	}
}
