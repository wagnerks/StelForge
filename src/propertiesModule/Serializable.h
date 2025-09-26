#pragma once

#include <json/value.h>

namespace SFE::PropertiesModule {
	//todo serialization should work without serializable object virtualization
	class Serializable {
	public:
		virtual ~Serializable() = default;

		virtual void serialize(Json::Value& data) = 0;
		virtual void deserialize(const Json::Value& data) = 0;
	};
}
