#pragma once
#include <string>


namespace ecsModule {
	class EntityInterface;
}

namespace GameEngine {
	namespace ComponentsModule {
		class MeshComponent;
		class LightComponent;
		class ModelComponent;
		class TransformComponent;
		class LodComponent;
	}
}


namespace GameEngine::Debug {
	class ComponentsDebug {
	public:
		static void drawTree(ecsModule::EntityInterface* entity, size_t& selectedID);
		static void entitiesDebug();
		static void componentEditorInternal(ComponentsModule::TransformComponent* component);
		static void componentEditorInternal(ComponentsModule::LodComponent* component);
		static void componentEditorInternal(ComponentsModule::LightComponent* component);
		static void componentEditorInternal(ComponentsModule::MeshComponent* component);
		
		static inline size_t mSelectedId = std::numeric_limits<size_t>::max();
	};

}
