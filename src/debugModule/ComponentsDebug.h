#pragma once
#include <string>

#include "componentsModule/TransformComponent.h"


namespace GameEngine::Debug {
	class ComponentsDebug {
	public:
		static void transformComponentDebug(std::string_view id, TransformComponent* transformComp);
	};

}
