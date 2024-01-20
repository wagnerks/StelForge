#pragma once
#include <vector>

#include "systemsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class ShaderSystem : public ecss::System {
	public:
		void update(float_t dt) override;

		std::vector<std::pair<size_t, ecss::SectorId>> drawableEntities;
	};
}
