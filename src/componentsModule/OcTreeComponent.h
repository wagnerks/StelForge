#pragma once
#include <vector>
#include <mathModule/Forward.h>

#include "core/ECSHandler.h"
#include "systemsModule/systems/OcTreeSystem.h"

class OcTreeComponent {
public:

	bool inOctree = false;
	std::vector<SFE::Math::Vec3> mParentOcTrees; //vector because it is possible that object is VERY big, or on the border
};
