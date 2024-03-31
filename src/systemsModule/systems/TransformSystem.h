#pragma once

#include <vector>

#include "systemsModule/SystemBase.h"
#include "systemsModule/TasksManager.h"

namespace SFE::SystemsModule {
	class TransformSystem : public ecss::System {
	public:
		TransformSystem() : ecss::System({TRANSFORM_UPDATE}) {}
		void updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) override;
	};
}
