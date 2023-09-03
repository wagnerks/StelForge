#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Model.h"

namespace Engine::ComponentsModule{
	class MaterialComponent : public ecsModule::Component<MaterialComponent> {
	public:
		void addTexture(const ModelModule::Material& texture);
		void removeTexture(const ModelModule::Material& texture);
	private:
		ModelModule::Material mMaterial;
	};
}

using Engine::ComponentsModule::MaterialComponent;