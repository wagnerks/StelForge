#pragma once
#include <string>

#include "componentsModule/TransformComponent.h"


namespace GameEngine {
	namespace CoreModule {
		class Node;

	}
}

namespace GameEngine::Debug {
	class ComponentsDebug {
	public:
		static void transformComponentDebug(std::string_view id, ComponentsModule::TransformComponent* transformComp);
	};

}
