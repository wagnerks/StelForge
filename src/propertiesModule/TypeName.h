#pragma once
#include <string>

#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/TransformComponent.h"

namespace Engine::PropertiesModule {
	// TypeTraits to associate a type with its name
	template <typename T>
	struct TypeName {
		static std::string_view name() {
			assert(false);
			return "unknown";
		}
	};

	// Specializations of TypeName for specific types
	template <>
	struct TypeName<TransformComponent> {
		static std::string_view name() {
			return "TransformComponent";
		}
	};

	template <>
	struct TypeName<ModelComponent> {
		static std::string_view name() {
			return "ModelComponent";
		}
	};

	template <>
	struct TypeName<CascadeShadowComponent> {
		static std::string_view name() {
			return "CascadeShadowComponent";
		}
	};
}
