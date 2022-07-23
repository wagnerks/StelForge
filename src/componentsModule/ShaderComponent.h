#pragma once
#include "Component.h"
#include "componentsModule/ComponentHolder.h"
#include "shaderModule/Shader.h"

namespace GameEngine::ComponentsModule{
	class ShaderComponent : public Component {
	public:
		ShaderComponent(ComponentHolder* holder) : Component(holder){}
		void updateComponent() override {};
		void setShader(ShaderModule::ShaderBase* aShader);
		ShaderModule::ShaderBase* getShader();
		void bind();
	private:
		ShaderModule::ShaderBase* shader = nullptr;
	};
}

using GameEngine::ComponentsModule::ShaderComponent;