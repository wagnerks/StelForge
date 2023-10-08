#pragma once
#include "propertiesModule/PropertiesSystem.h"
#include "core/FileSystem.h"

#include "TypeName.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"

namespace Engine::PropertiesModule {
	ecss::EntityHandle PropertiesSystem::loadScene(std::string_view path) {
		if (!FileSystem::isFileExists(path)) {
			return {};
		}

		auto scene = ECSHandler::registry()->takeEntity();

		fillTree(scene, FileSystem::readJson(path));

		return scene;
	}

	void PropertiesSystem::applyProperties(const ecss::EntityHandle& entity, const Json::Value& properties) {
		if (!entity) {
			return;
		}
		if (!properties.isMember("Properties")) {
			return;
		}

		deserializeProperty<TransformComponent>(entity, properties["Properties"]);
		deserializeProperty<ModelComponent>(entity, properties["Properties"]);
	}

	void PropertiesSystem::fillTree(const ecss::EntityHandle& entity, const Json::Value& properties) {
		if (!properties.isObject()) {
			return;
		}

		if (properties.isMember("id")) {
			auto debugData = ECSHandler::registry()->addComponent<DebugDataComponent>(entity);
			debugData->stringId = properties["id"].asString();
		}

		ECSHandler::registry()->addComponent<IsDrawableComponent>(entity);
		applyProperties(entity, properties);

		auto tr = ECSHandler::registry()->getComponent<TransformComponent>(entity);

		if (properties.isMember("Children") && properties["Children"].isArray()) {
			for (auto element : properties["Children"]) {
				auto child = ECSHandler::registry()->takeEntity();
				ECSHandler::registry()->addComponent<IsDrawableComponent>(child);
				fillTree(child, element);
				auto treeComp = ECSHandler::registry()->addComponent<ComponentsModule::TreeComponent>(entity);
				treeComp->addChildEntity(child.getID());
			}
		}
	}

	Json::Value PropertiesSystem::serializeEntity(const ecss::EntityHandle& entity) {
		Json::Value result = Json::objectValue;
		if (!entity) {
			return result;
		}

		if (auto debugData = ECSHandler::registry()->getComponent<DebugDataComponent>(entity)) {
			result["id"] = debugData->stringId;
		}
		

		serializeProperty<TransformComponent>(entity, result["Properties"]);
		serializeProperty<ModelComponent>(entity, result["Properties"]);
		serializeProperty<CascadeShadowComponent>(entity, result["Properties"]);

		auto treeComp = ECSHandler::registry()->getComponent<ComponentsModule::TreeComponent>(entity);
		auto children = treeComp ? treeComp->getChildren() : std::vector<ecss::EntityId>();
		if (!children.empty()) {
			result["Children"] = Json::arrayValue;
			auto& childrenJson = result["Children"];
			for (auto node : children) {
				childrenJson.append(serializeEntity(ECSHandler::registry()->getEntity(node)));
			}
		}

		return result;
	}
}
