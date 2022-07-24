#pragma once
#include "Component.h"

namespace GameEngine::ComponentsModule{
	class DrawComponent : public Component{
	public:
		DrawComponent(ComponentHolder* holder): Component(holder) {}

		void updateComponent() override {};
		void draw();
	private:


	};
}

using GameEngine::ComponentsModule::DrawComponent;