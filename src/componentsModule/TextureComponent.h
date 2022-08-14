#pragma once
#include <vector>

#include "ecsModule/ComponentBase.h"
#include "modelModule/Model.h"

namespace GameEngine::ComponentsModule{
	class TextureComponent : public ecsModule::Component<TextureComponent> {
	public:
		void addTexture(const ModelModule::ModelTexture& texture);
		void removeTexture(const ModelModule::ModelTexture& texture);
	private:
		std::vector<ModelModule::ModelTexture> textures;
	};
}

using GameEngine::ComponentsModule::TextureComponent;