#pragma once
#include <vector>

#include "ComponentBase.h"
#include "assetsModule/modelModule/Model.h"
#include "ecss/Types.h"

namespace Engine::ComponentsModule {
	class MaterialComponent : public ecss::ComponentInterface {
	public:
		MaterialComponent(ecss::SectorId id) : ComponentInterface(id) {};
		void addTexture(const AssetsModule::Material& texture);
		void removeTexture(const AssetsModule::Material& texture);
	private:
		AssetsModule::Material mMaterial;
	};
}

using Engine::ComponentsModule::MaterialComponent;