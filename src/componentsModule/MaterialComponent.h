#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"
#include "assetsModule/modelModule/Model.h"

namespace Engine::ComponentsModule {
	class MaterialComponent : public ecsModule::Component<MaterialComponent> {
	public:
		void addTexture(const AssetsModule::Material& texture);
		void removeTexture(const AssetsModule::Material& texture);
	private:
		AssetsModule::Material mMaterial;
	};
}

using Engine::ComponentsModule::MaterialComponent;