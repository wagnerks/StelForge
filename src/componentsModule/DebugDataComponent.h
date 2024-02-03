#pragma once
#include <string>

#include "componentsModule/ComponentBase.h"

namespace SFE::ComponentsModule {

	class DebugDataComponent {
	public:
		DebugDataComponent() = default;
		std::string stringId;
	};
}

using SFE::ComponentsModule::DebugDataComponent;
