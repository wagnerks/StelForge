#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"


namespace GameEngine::ComponentsModule{
	class DrawComponent : public ecsModule::Component<DrawComponent>{
	public:
		void draw();
	private:
	};
}

using GameEngine::ComponentsModule::DrawComponent;