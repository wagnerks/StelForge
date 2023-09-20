#pragma once

#include "ecsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class OutlineComponent : public ecsModule::Component<OutlineComponent> {}; //
}

using Engine::ComponentsModule::OutlineComponent;
