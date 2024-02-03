#include "ShaderController.h"

#include <ranges>

#include "GeometryShader.h"
#include "Shader.h"
#include "glad/glad.h"

using namespace SFE;
using namespace SFE::ShaderModule;

ShaderController::ShaderController() = default;

ShaderController::~ShaderController() {
	for (auto& [hash, shader] : shaders) {
		delete shader;
	}

	shaders.clear();
}

void ShaderController::init() {
}

ShaderBase* ShaderController::loadVertexFragmentShader(const std::string& vertexPath, const std::string& fragmentPath) {
	size_t hash = hasher(vertexPath + fragmentPath);
	const auto it = shaders.find(hash);
	if (it != shaders.end()) {
		return it->second;
	}
	auto shader = new Shader(vertexPath.c_str(), fragmentPath.c_str(), hash);
	shader->compile();
	return shaders.emplace(hash, shader).first->second;
}

ShaderBase* ShaderController::loadGeometryShader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath) {
	size_t hash = hasher(geometryPath);
	const auto it = shaders.find(hash);
	if (it != shaders.end()) {
		return it->second;
	}
	auto shader = new GeometryShader(vertexPath.c_str(), fragmentPath.c_str(), geometryPath.c_str(), hash);
	shader->compile();
	return shaders.emplace(hash, shader).first->second;
}

void ShaderController::recompileShader(ShaderBase* shader) {
	deleteShaderGL(shader->getID());
	shader->compile();
}

void ShaderController::initDefaultShader() {
	defaultShader = loadVertexFragmentShader("shaders/main.vs", "shaders/main.fs");
	useDefaultShader();
}


void ShaderController::useShader(unsigned ID) {
	if (currentShader != ID) {
		glUseProgram(ID);
		currentShader = ID;
	}
}

void ShaderController::useDefaultShader() {
	initDefaultShader();
	useShader(defaultShader->getID());
}

void ShaderController::deleteShaderGL(unsigned ID) {
	if (currentShader == ID) {
		glUseProgram(0);
		currentShader = 0;
	}
	glDeleteProgram(ID);
}

void ShaderController::deleteShader(ShaderBase* shader) {
	removeShader(shader);
	delete shader;
	shader = nullptr;
}

void ShaderController::deleteShader(size_t shaderHash) {
	if (shaders.contains(shaderHash)) {
		deleteShader(shaders[shaderHash]);
	}
}

void ShaderController::removeShader(ShaderBase* shader) {
	if (!shader) {
		return;
	}

	shaders.erase(shader->mHash);
}

const std::unordered_map<size_t, ShaderBase*>& ShaderController::getShaders() {
	return shaders;
}

ShaderBase* ShaderController::getShader(size_t shaderID) {
	for (auto shader : shaders) {
		if (shader.second->getID() == shaderID) {
			return shader.second;
		}
	}

	return nullptr;
}