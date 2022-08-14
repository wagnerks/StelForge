#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"


namespace GameEngine::ComponentsModule{
	class RenderComponent : public ecsModule::Component<RenderComponent>{
	public:
		bool isDrawable() const;
		void setDrawable(bool isDrawable);
	private:
		bool mIsDrawable = true;
	};
}

using GameEngine::ComponentsModule::RenderComponent;