#include "Shader.h"

#include <filesystem>
#include <map>
#include <glad/glad.h>
#include <string>
#include "logsModule/logger.h"

using namespace Engine;
using namespace Engine::ShaderModule;


Shader::Shader(const char* vertexPath, const char* fragmentPath, size_t hash) : ShaderBase(hash), vertexPath(vertexPath), fragmentPath(fragmentPath) {
}

bool Shader::compile() {
	cachedUniforms.clear();
	ID = glCreateProgram();

	vertexCode = loadShaderCode(vertexPath);
	fragmentCode = loadShaderCode(fragmentPath);

	auto success = compileShader(vertexCode.c_str(), GL_VERTEX_SHADER);
	success = compileShader(fragmentCode.c_str(), GL_FRAGMENT_SHADER) && success;
	if (!success) {
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


