#pragma once
#include "Component.h"
#include "nodeModule/Node.h"

namespace GameEngine::ComponentsModule {
	class NodeComponent : public Component, public NodeModule::Node { //todo draft - need think about node architecture, and components/entities updates every frame
	public:
		void updateComponent() override;
		
	};	
}

