#include "Shader.h"

#include <filesystem>
#include <glad/glad.h>
#include <string>
#include "logsModule/logger.h"

using namespace GameEngine;
using namespace GameEngine::ShaderModule;


Shader::Shader(const char* vertexPath, const char* fragmentPath, size_t hash) : ShaderBase(hash), vertexPath(vertexPath), fragmentPath(fragmentPath) {
}

bool Shader::compile() {
	cachedUniforms.clear();
	ID = glCreateProgram();

	auto success = compileShader(loadShaderCode(vertexPath.c_str()).c_str(), GL_VERTEX_SHADER);
	success = compileShader(loadShaderCode(fragmentPath.c_str()).c_str(), GL_FRAGMENT_SHADER) && success;
	if (!success){
		LogsModule::Logger::LOG_ERROR("[%s, %s] error downloading", vertexPath.c_str(), fragmentPath.c_str());
	}
	return success;
}

std::string_view Shader::getVertexPath() {
	return vertexPath;
}

std::string_view Shader::getFragmentPath() {
	return fragmentPath;
}


