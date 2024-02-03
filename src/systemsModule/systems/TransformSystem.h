#pragma once

#include <vector>

#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class TransformSystem : public ecss::System {
	public:
		void update(const std::vector<ecss::SectorId>& entitiesToProcess) override;

		void addDirtyComp(ecss::SectorId entity);
	};
}