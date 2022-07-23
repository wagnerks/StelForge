#include "Shader.h"

#include <filesystem>
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <gtc/type_ptr.hpp>

#include "ShaderController.h"
#include "logsModule/logger.h"

using namespace GameEngine;
using namespace GameEngine::ShaderModule;


Shader::Shader(const char* vertexPath, const char* fragmentPath, size_t hash) : ShaderBase(hash), vertexPath(vertexPath), fragmentPath(fragmentPath) {
}

unsigned ShaderBase::getID() const {
	return ID;
}

void ShaderBase::use() const {
	ShaderController::getInstance()->useShader(getID());
}

void ShaderBase::setInt(const char* name, int val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform1i(getUniformLocation(name), val);
}

void ShaderBase::setMat4(const char* name, const glm::mat4& val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void ShaderBase::setVec2(const char* name, const glm::vec2& val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(val));
}

void ShaderBase::setVec3(const char* name, const glm::vec3& val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(val));
}

void ShaderBase::setFloat(const char* name, float val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform1f(getUniformLocation(name), val);
}

bool ShaderBase::checkCompileErrors(unsigned int shader, std::string_view type) {
	int success;
	char infoLog[1024];
	if (type != "PROGRAM") {
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);

			LogsModule::Logger::LOG_ERROR("SHADER_COMPILATION_ERROR of type: %s\n%s\n-- --------------------------------------------------- --", type.data(), infoLog);
		}
	}
	else {
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);

			LogsModule::Logger::LOG_ERROR("PROGRAM_LINKING_ERROR of type: %s\n%s\n-- --------------------------------------------------- --", type.data(), infoLog);
		}
	}

	return success;
}

int ShaderBase::getUniformLocation(const char* name) {
	const auto found = cachedUniforms.find(name);
	if (found != cachedUniforms.end()) {
		return found->second;
	}

	return cachedUniforms.insert({name, glGetUniformLocation(ID, name)}).first->second;
}

bool Shader::compile() {
	cachedUniforms.clear();
	ID = glCreateProgram();

	auto success = compileShader(loadShaderCode(vertexPath.c_str()).c_str(), GL_VERTEX_SHADER);
	success = compileShader(loadShaderCode(fragmentPath.c_str()).c_str(), GL_FRAGMENT_SHADER) || success;
	return success;
}

std::string_view Shader::getVertexPath() {
	return vertexPath;
}

std::string_view Shader::getFragmentPath() {
	return fragmentPath;
}

bool ShaderBase::compileShader(const char* shaderCode, unsigned type) {
	const auto shader = glCreateShader(type);
	glShaderSource(shader, 1, &shaderCode, nullptr);
	glCompileShader(shader);

	auto success = checkCompileErrors(shader, "VERTEX");

	glAttachShader(ID, shader);

	glLinkProgram(ID);
	success = checkCompileErrors(ID, "PROGRAM") && success;

	glDeleteShader(shader);

	return success;
}

std::string ShaderBase::loadShaderCode(const char* path) {
	std::ifstream shaderFile;

	std::string shaderCode;

	shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try {
		shaderFile.open(path);
		std::stringstream shaderStream;

		shaderStream << shaderFile.rdbuf();

		shaderFile.close();

		shaderCode = shaderStream.str();
	}
	catch (std::ifstream::failure& e) {
		LogsModule::Logger::LOG_ERROR("SHADER::FILE_NOT_SUCCESSFULLY_READ: %s, \npath: %s", e.what(), path);
		return {};
	}

	return shaderCode;
}
