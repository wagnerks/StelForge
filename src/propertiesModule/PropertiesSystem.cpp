#pragma once
#include "propertiesModule/PropertiesSystem.h"
#include "core/FileSystem.h"

#include "TypeName.h"
#include "ecsModule/EntityManager.h"
#include "..\entitiesModule\Object.h"
#include "componentsModule/CascadeShadowComponent.h"

namespace Engine::PropertiesModule {
	ecsModule::EntityInterface* PropertiesSystem::loadScene(std::string_view path) {
		if (!FileSystem::isFileExists(path)) {
			return nullptr;
		}

		auto scene = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Object>();
		fillTree(scene, FileSystem::readJson(path));

		return scene;
	}

	void PropertiesSystem::applyProperties(ecsModule::EntityInterface* entity, const Json::Value& properties) {
		if (!entity) {
			return;
		}
		if (!properties.isMember("Properties")) {
			return;
		}

		deserializeProperty<TransformComponent>(entity, properties["Properties"]);
		deserializeProperty<ModelComponent>(entity, properties["Properties"]);
	}

	void PropertiesSystem::fillTree(ecsModule::EntityInterface* entity, const Json::Value& properties) {
		if (!properties.isObject()) {
			return;
		}

		if (properties.isMember("id")) {
			entity->setNodeId(properties["id"].asString());
		}

		applyProperties(entity, properties);

		if (properties.isMember("Children") && properties["Children"].isArray()) {
			for (auto element : properties["Children"]) {
				auto child = ecsModule::ECSHandler::entityManagerInstance()->createEntity<EntitiesModule::Object>();
				entity->addElement(child);
				fillTree(child, element);
			}
		}
	}

	Json::Value PropertiesSystem::serializeEntity(ecsModule::EntityInterface* entity) {
		Json::Value result = Json::objectValue;
		if (!entity) {
			return result;
		}

		result["id"] = entity->getNodeId().data();

		serializeProperty<TransformComponent>(entity, result["Properties"]);
		serializeProperty<ModelComponent>(entity, result["Properties"]);
		serializeProperty<CascadeShadowComponent>(entity, result["Properties"]);

		auto children = entity->getElements();
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
