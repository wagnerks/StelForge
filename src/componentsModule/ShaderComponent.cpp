#include "ShaderComponent.h"

#include "shaderModule/Shader.h"

using namespace GameEngine::ComponentsModule;

void ShaderComponent::setShader(ShaderModule::ShaderBase* aShader) {
	shader = aShader;
}

GameEngine::ShaderModule::ShaderBase* ShaderComponent::getShader() {
	return shader;
}

void ShaderComponent::bind() {
	if (shader) {
		shader->use();
	}
}
