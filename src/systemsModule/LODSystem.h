#pragma once
#include "ecsModule/SystemBase.h"

namespace AssetsModule {
	class Mesh;
}

namespace Engine {
	namespace ComponentsModule {
		class TransformComponent;
	}
}

class Camera;

namespace Engine::SystemsModule {
	class LODSystem : public ecsModule::System<LODSystem> {
	public:
		void preUpdate(float_t dt) override {}
		void update(float_t dt) override;
		void postUpdate(float_t dt) override {}

		static float calculateScreenSpaceArea(const AssetsModule::Mesh* mesh, const Camera* camera, ComponentsModule::TransformComponent* meshTransform);
		static float calculateDistanceToMesh(const Camera* camera, ComponentsModule::TransformComponent* meshTransform);
	private:

	};
}
