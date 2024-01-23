#pragma once
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class AABBSystem : public ecss::System {
	public:
		void update(const std::vector<ecss::SectorId>& entitiesToProcess) override;
	};
}
