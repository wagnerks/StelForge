#pragma once

#include "core/InputHandler.h"
#include "ecss/Types.h"

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
		class PhysicsComponent;
		class ShaderComponent;
	}
}


namespace Engine::Debug {
	class ComponentsDebug : public Singleton<ComponentsDebug>, public CoreModule::InputObserver {
	public:
		void init() override;
		void drawTree(const ecss::EntityHandle& entity, ecss::SectorId& selectedID);
		void entitiesDebug();
		void componentEditorInternal(ComponentsModule::TransformComponent* component);
		void componentEditorInternal(ComponentsModule::LightSourceComponent* component);
		void componentEditorInternal(ComponentsModule::CascadeShadowComponent* component);
		void componentEditorInternal(ComponentsModule::ModelComponent* component);
		void componentEditorInternal(ComponentsModule::ShaderComponent* component);
		void componentEditorInternal(ComponentsModule::PhysicsComponent* component);

		ecss::SectorId mSelectedId = ecss::INVALID_ID;

		bool editorOpened = true;
	};

}
