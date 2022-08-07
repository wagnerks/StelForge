#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"


namespace GameEngine::ComponentsModule{
	class RenderComponent : public ecsModule::Component<RenderComponent>{
	public:
		void draw();
	private:
	};
}

using GameEngine::ComponentsModule::RenderComponent;