#pragma once
#include "systemsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class AABBSystem : public ecss::System {
	public:
		void update(const std::vector<ecss::SectorId>& entitiesToProcess) override;
	};
}
