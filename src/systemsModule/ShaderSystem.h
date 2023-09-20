#pragma once
#include <vector>

#include "ecsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class ShaderSystem : public ecsModule::System<ShaderSystem> {
	public:
		void update(float_t dt) override;

		std::vector<std::pair<size_t, size_t>> drawableEntities;
	};
}
