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

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::DebugDataComponent> : std::true_type {};

using SFE::ComponentsModule::DebugDataComponent;
