#pragma once
#include <string>

#include "componentsModule/ComponentBase.h"

namespace Engine::ComponentsModule {

	class DebugDataComponent {
	public:
		DebugDataComponent() = default;
		std::string stringId;
	};
}

using Engine::ComponentsModule::DebugDataComponent;
