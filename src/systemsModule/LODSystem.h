#pragma once
#include <cmath>


#include "ecss/base/EntityHandle.h"
#include "SystemBase.h"

namespace AssetsModule {
	class Mesh;
}

namespace Engine {
	namespace ComponentsModule {
		class TransformComponent;
	}
}


namespace Engine::SystemsModule {
	class LODSystem : public ecss::System<LODSystem> {
	public:
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override {}

		static float calculateScreenSpaceArea(const AssetsModule::Mesh* mesh, const ecss::EntityHandle& camera, ComponentsModule::TransformComponent* meshTransform);
		static float calculateDistanceToMesh(const ecss::EntityHandle& camera, ComponentsModule::TransformComponent* meshTransform);
	private:

	};
}
