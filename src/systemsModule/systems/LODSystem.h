#pragma once

#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

namespace Engine::SystemsModule {
	class LODSystem : public ecss::System {
	public:
		void update(float_t dt) override;

		//static float calculateScreenSpaceArea(const AssetsModule::Mesh* mesh, const ecss::EntityHandle& camera, ComponentsModule::TransformComponent* meshTransform);
		static float calculateDistanceToMesh(const Math::Vec3& cameraPos, const Math::Vec3& meshPos);
	};
}
