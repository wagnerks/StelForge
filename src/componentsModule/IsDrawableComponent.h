#pragma once

#include "ecsModule/ComponentBase.h"

namespace Engine::ComponentsModule {
	class IsDrawableComponent : public ecsModule::Component<IsDrawableComponent> {}; //
}

using Engine::ComponentsModule::IsDrawableComponent;