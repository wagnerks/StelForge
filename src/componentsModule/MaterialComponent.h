#pragma once
#include <vector>

#include "assetsModule/modelModule/Model.h"

namespace Engine::ComponentsModule {
	class MaterialComponent : public ecss::ComponentInterface {
	public:
		MaterialComponent(ecss::EntityId id) : ComponentInterface(id) {};
		void addTexture(const AssetsModule::Material& texture);
		void removeTexture(const AssetsModule::Material& texture);
	private:
		AssetsModule::Material mMaterial;
	};
}

using Engine::ComponentsModule::MaterialComponent;