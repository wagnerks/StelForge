#pragma once
#include <string>

#include "componentsModule/ShaderComponent.h"


namespace ecsModule {
	class EntityInterface;
}

namespace Engine {
	namespace ComponentsModule {
		class LightSourceComponent;
		class ModelComponent;
		class TransformComponent;
		class LodComponent;
		class CascadeShadowComponent;
	}
}


namespace Engine::Debug {
	class ComponentsDebug {
	public:
		static void drawTree(ecsModule::EntityInterface* entity, size_t& selectedID);
		static void entitiesDebug();
		static void componentEditorInternal(ComponentsModule::TransformComponent* component);
		static void componentEditorInternal(ComponentsModule::LightSourceComponent* component);
		static void componentEditorInternal(ComponentsModule::CascadeShadowComponent* component);
		static void componentEditorInternal(ComponentsModule::ModelComponent* component);
		static void componentEditorInternal(ComponentsModule::ShaderComponent* component);

		static inline size_t mSelectedId = std::numeric_limits<size_t>::max();
	};

}
