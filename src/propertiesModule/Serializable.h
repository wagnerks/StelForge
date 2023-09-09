#pragma once

#include <json/value.h>

namespace Engine::PropertiesModule {
	class Serializable {
	public:
		virtual ~Serializable() = default;

		virtual bool serialize(Json::Value& data) = 0;
		virtual bool deserialize(const Json::Value& data) = 0;
	};
}
