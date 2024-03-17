#pragma once

#include "core/InputHandler.h"
#include "ecss/Types.h"
#include "renderModule/Gizmo.h"

namespace SFE {
	namespace ComponentsModule {
		struct OcclusionComponent;
		struct AABBComponent;
		class LightSourceComponent;
		class ModelComponent;
		class TransformComponent;
		class LodComponent;
		class CascadeShadowComponent;
		class PhysicsComponent;
		class ShaderComponent;
		struct AnimationComponent;
	}
}


namespace SFE::Debug {
	class ComponentsDebug : public Singleton<ComponentsDebug>, public CoreModule::InputObserver {
	public:
		void init() override;
		void drawTree(ecss::EntityId entity, ecss::SectorId& selectedID);
		void entitiesDebug();

		void editComponentGui(ComponentsModule::CameraComponent* component);
		void editComponentGui(ComponentsModule::TransformComponent* component);
		void editComponentGui(ComponentsModule::LightSourceComponent* component);
		void editComponentGui(ComponentsModule::CascadeShadowComponent* component);
		void editComponentGui(ComponentsModule::ModelComponent* component);
		void editComponentGui(ComponentsModule::ShaderComponent* component);
		void editComponentGui(ComponentsModule::PhysicsComponent* component);
		void editComponentGui(ComponentsModule::AnimationComponent* component);
		void editComponentGui(ComponentsModule::AABBComponent* component);
		void editComponentGui(ComponentsModule::OcclusionComponent* component);


		void entitiesTreeGUI();
		void componentGUI();
		void entitiesMenuGUI();

		template<typename CompType>
		void componentEditImpl(ecss::EntityId id);

		ecss::EntityId mSelectedId = ecss::INVALID_ID;
		ecss::EntityId mCameraId = ecss::INVALID_ID;

		bool editorOpened = true;

		std::map<CoreModule::InputKey, bool> pressedKeys;
		Render::Gizmo gizmo;

		void setSelectedId(ecss::EntityId id);
	};

	
}
