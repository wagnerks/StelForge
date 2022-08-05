#pragma once
#include "ecsModule/ComponentBase.h"
#include "nodeModule/Node.h"

namespace GameEngine::ComponentsModule {
	class NodeComponent : public ecsModule::Component<NodeComponent>, public NodeModule::Node { //todo draft - need think about node architecture, and components/entities updates every frame
	public:
	};	
}

