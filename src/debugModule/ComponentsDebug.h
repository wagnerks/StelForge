#pragma once
#include <string>


namespace GameEngine {
	namespace ComponentsModule {
		class ModelComponent;
		class TransformComponent;
	}
}

class LodComponent;

namespace GameEngine::Debug {
	class ComponentsDebug {
	public:
		static void transformComponentDebug(std::string_view id, ComponentsModule::TransformComponent* transformComp);

		static void entitiesDebug();
		static void transformComponentInternal(ComponentsModule::TransformComponent* component);
		static void lodComponentInternal(LodComponent* component);
		static void modelComponentInternal(ComponentsModule::ModelComponent* component);
	};

}
