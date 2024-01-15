#pragma once
#include "ComponentBase.h"
#include "core/BoundingVolume.h"
#include "core/Projection.h"
#include "ecss/Types.h"

namespace Engine::ComponentsModule {
	class FrustumComponent : public ecss::ComponentInterface {
	public:
		FrustumComponent(ecss::SectorId id) : ComponentInterface(id) {};
		void updateFrustum(const ProjectionModule::Projection& projection, const Math::Mat4& view);
		void updateFrustum(const Math::Mat4& projView);
		void updateFrustum(const Math::Mat4& projection, const Math::Mat4& view);
		bool isOnFrustum(const FrustumModule::BoundingVolume& bVolume) const;
		FrustumModule::Frustum* getFrustum();
	private:
		FrustumModule::Frustum mFrustum;
	};
}

using Engine::ComponentsModule::FrustumComponent;