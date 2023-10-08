#pragma once
#include <string>

#include "componentsModule/ShaderComponent.h"


namespace ecss {
	class EntityHandle;
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
		static void drawTree(const ecss::EntityHandle& entity, ecss::EntityId& selectedID);
		static void entitiesDebug();
		static void componentEditorInternal(ComponentsModule::TransformComponent* component);
		static void componentEditorInternal(ComponentsModule::LightSourceComponent* component);
		static void componentEditorInternal(ComponentsModule::CascadeShadowComponent* component);
		static void componentEditorInternal(ComponentsModule::ModelComponent* component);
		static void componentEditorInternal(ComponentsModule::ShaderComponent* component);

		static inline ecss::EntityId mSelectedId = ecss::INVALID_ID;
	};

}
