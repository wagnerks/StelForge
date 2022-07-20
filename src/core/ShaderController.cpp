#include "ShaderController.h"

#include <ranges>

#include "shader.h"

using namespace GameEngine;
using namespace GameEngine::Render;

ShaderController::ShaderController() = default;

ShaderController::~ShaderController() {
	for (const auto& shader : shaders | std::views::values) {
		delete shader;
	}

	shaders.clear();
}

void ShaderController::init() {
	initDefaultShader();
}

Shader* ShaderController::loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
	auto it = shaders.find(vertexPath + fragmentPath);
	if (it != shaders.end()) {
		return it->second;
	}

	auto shader = new Shader(vertexPath.c_str(), fragmentPath.c_str());
	shaders[vertexPath + fragmentPath] = shader;

	return shader;
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

void ShaderController::deleteShader(unsigned ID) {
	if (currentShader == ID) {
		glUseProgram(0);
		currentShader = 0;
	}
	glDeleteProgram(ID);
}

void ShaderController::removeShader(const std::string& hash) {
	shaders.erase(hash);
}

const std::unordered_map<std::string, Shader*>& ShaderController::getShaders() {
	return shaders;
}
