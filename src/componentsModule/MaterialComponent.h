#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Model.h"

namespace GameEngine::ComponentsModule{
	class MaterialComponent : public ecsModule::Component<MaterialComponent> {
	public:
		void addTexture(const ModelModule::Material& texture);
		void removeTexture(const ModelModule::Material& texture);
	private:
		ModelModule::Material mMaterial;
	};
}

using GameEngine::ComponentsModule::MaterialComponent;