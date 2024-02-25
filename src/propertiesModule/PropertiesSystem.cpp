#include "propertiesModule/PropertiesSystem.h"
#include "core/FileSystem.h"

#include "TypeName.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/TreeComponent.h"
#include "core/ECSHandler.h"
#include "core/ThreadPool.h"

namespace SFE::PropertiesModule {
	ecss::EntityHandle PropertiesSystem::loadScene(std::string_view path) {
		if (!FileSystem::isFileExists(path)) {
			return {};
		}

		auto scene = ECSHandler::registry().takeEntity();

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

		deserializeProperty<ModelComponent>(entity, properties["Properties"]);
		deserializeProperty<TransformComponent>(entity, properties["Properties"]);
	}

	void PropertiesSystem::fillTree(const ecss::EntityHandle& entity, const Json::Value& properties) {
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

		auto treeComp = ECSHandler::registry().addComponent<TreeComponent>(entity, entity.getID());

		if (properties.isMember("Children") && properties["Children"].isArray()) {
			for (auto element : properties["Children"]) {
				auto child = ECSHandler::registry().takeEntity();				
				fillTree(child, element);
				treeComp->addChildEntity(child.getID());
			}
		}
	}

	Json::Value PropertiesSystem::serializeEntity(const ecss::EntityHandle& entity) {
		Json::Value result = Json::objectValue;
		if (!entity) {
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
				childrenJson.append(serializeEntity(ECSHandler::registry().getEntity(node)));
			}
		}

		return result;
	}
}
