#include "ShaderController.h"

#include <ranges>

#include "Shader.h"

using namespace GameEngine;
using namespace GameEngine::ShaderModule;

ShaderController::ShaderController() = default;

ShaderController::~ShaderController() {
	shaders.clear();
}

void ShaderController::init() {
	initDefaultShader();
}

Shader* ShaderController::loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
	size_t hash = hasher(vertexPath +  fragmentPath);
	const auto it = shaders.find(hash);
	if (it != shaders.end()) {
		return it->second;
	}
	auto shader = new Shader(vertexPath.c_str(), fragmentPath.c_str(), hash);
	shaderPaths[hash] = {vertexPath, fragmentPath};
	return shaders.emplace(hash, shader).first->second;
}

void ShaderController::recompileShader(Shader* shader) {
	deleteShaderGL(shader->getID());
	const auto code = Shader::loadShaderCode(shaderPaths[shader->hash].first.c_str(), shaderPaths[shader->hash].second.c_str());
	shader->compileShader(code.first.c_str(), code.second.c_str());
}

ShaderController* ShaderController::getInstance() {
	if (!instance) {
		instance = new ShaderController();
		instance->init();
	}
	return instance;
}

void ShaderController::terminate() {
	delete instance;
	instance = nullptr;
}

void ShaderController::initDefaultShader() {
	defaultShader = loadShader("shaders/main.vs", "shaders/main.fs");
	useDefaultShader();
}


void ShaderController::useShader(unsigned ID) {
	if (currentShader != ID) {
		glUseProgram(ID);
		currentShader = ID;
	}
}

void ShaderController::useDefaultShader() {
	useShader(defaultShader->getID());
}

void ShaderController::deleteShaderGL(unsigned ID) {
	if (currentShader == ID) {
		glUseProgram(0);
		currentShader = 0;
	}
	glDeleteProgram(ID);
}

void ShaderController::removeShader(Shader* shader) {
	if (!shader) {
		return;
	}

	shaders.erase(shader->hash);
}

const std::unordered_map<size_t, Shader*>& ShaderController::getShaders() {
	return shaders;
}

const std::unordered_map<size_t, std::pair<std::string, std::string>>& ShaderController::getShaderPaths() {
	return shaderPaths;
}
