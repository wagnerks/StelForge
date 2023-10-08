#pragma once
#include <string>

#include "ecss/base/ComponentBase.h"

namespace Engine::ComponentsModule {

	class DebugDataComponent : public ecss::Component<DebugDataComponent> {
	public:
		std::string stringId;
	};
}

using Engine::ComponentsModule::DebugDataComponent;
