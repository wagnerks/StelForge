#pragma once
#include "ecss/base/ComponentBase.h"


namespace Engine::ComponentsModule {
	class IsDrawableComponent : public ecss::Component<IsDrawableComponent> {}; //
}

using Engine::ComponentsModule::IsDrawableComponent;