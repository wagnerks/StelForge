#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Model.h"

class LodComponent : public ecsModule::Component<LodComponent> {
public:
	std::vector<GameEngine::ModelModule::Model*> modelLODs;
};
