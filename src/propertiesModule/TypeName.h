#pragma once

#include <string>
#include "assert.h"
#include "componentsModule/ActionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/LightSourceComponent.h"

namespace SFE::PropertiesModule {
#define TYPE_NAME(type)									\
	template <>											\
	struct TypeName<ComponentsModule::type> {			\
		constexpr static std::string_view name() {		\
			return #type;								\
		}												\
	};													\

	// TypeTraits to associate a type with its name
	template <typename T>
	struct TypeName {
		constexpr static std::string_view name() {
			assert(false && "unknown type name");
			return "unknown";
		}
	};

	// Specializations of TypeName for specific types
	TYPE_NAME(TransformComponent);
	TYPE_NAME(ModelComponent);
	TYPE_NAME(CascadeShadowComponent);
	TYPE_NAME(CameraComponent);
	TYPE_NAME(AnimationComponent);
	TYPE_NAME(ActionComponent);
	TYPE_NAME(LightSourceComponent);
}
