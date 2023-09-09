#pragma once
#include <json/value.h>

#include "TypeName.h"

namespace Engine::PropertiesModule {
	class PropertiesSystem {
	public:
		//ecsModule::EntityInterface* createEntity(Json::Value properties);

		static ecsModule::EntityInterface* loadScene(std::string_view path);

		static void applyProperties(ecsModule::EntityInterface* entity, const Json::Value& properties);

		static void fillTree(ecsModule::EntityInterface* entity, const Json::Value& properties);

		static Json::Value serializeEntity(ecsModule::EntityInterface* entity);

		template<class T>
		static void deserializeProperty(ecsModule::EntityInterface* entity, const Json::Value& properties);

		template<class T>
		static void serializeProperty(ecsModule::EntityInterface* entity, Json::Value& properties);
	private:
	};

	template <class T>
	void PropertiesSystem::deserializeProperty(ecsModule::EntityInterface* entity, const Json::Value& properties) {
		if (!entity) {
			return;
		}
		auto name = TypeName<T>::name().data();
		if (properties.isMember(name)) {
			entity->addComponent<T>()->deserialize(properties[name]);
		}
	}

	template <class T>
	void PropertiesSystem::serializeProperty(ecsModule::EntityInterface* entity, Json::Value& properties) {
		if (!entity) {
			return;
		}

		if (auto component = entity->getComponent<T>()) {
			component->serialize(properties[TypeName<T>::name().data()]);
		}

	}

	namespace JsonUtils {
		inline const Json::Value* getValue(const Json::Value& json, std::string_view memberId) {
			if (json.isMember(memberId.data())) {
				return &json[memberId.data()];
			}

			return nullptr;
		}

		inline const Json::Value* getValueArray(const Json::Value& json, std::string_view memberId) {
			if (auto val = getValue(json, memberId)) {
				if (val->isArray()) {
					return val;
				}
			}

			return nullptr;
		}

		inline glm::vec3 getVec3(const Json::Value& json) {
			if (json.isArray()) {
				return { json[0].asFloat(), json[1].asFloat(), json[2].asFloat() };
			}
			return {};
		}

		inline glm::vec2 getVec2(const Json::Value& json) {
			if (json.isArray()) {
				return { json[0].asFloat(), json[1].asFloat() };
			}
			return {};
		}
	}
}
