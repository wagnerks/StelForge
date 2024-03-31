#pragma once
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class AABBSystem : public ecss::System {
	public:
		AABBSystem() : System({ SFE::SystemsModule::TaskType::TRAHSFORM_RELOADED }){}
		void updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) override;
	};
}
