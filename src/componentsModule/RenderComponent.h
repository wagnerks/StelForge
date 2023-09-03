#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"


namespace Engine::ComponentsModule{
	class RenderComponent : public ecsModule::Component<RenderComponent>{
	public:
		bool isDrawable() const;
		void setDrawable(bool isDrawable);
	private:
		bool mIsDrawable = true;
	};
}

using Engine::ComponentsModule::RenderComponent;