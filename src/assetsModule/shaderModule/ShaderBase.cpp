#include "ShaderBase.h"

#include <fstream>
#include <sstream>
#include <glad/glad.h>
#include <gtc/type_ptr.hpp>

#include "ShaderController.h"
#include "core/FileSystem.h"
#include "logsModule/logger.h"

using namespace Engine::ShaderModule;

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

std::string ShaderBase::loadShaderCode(std::string_view path) {

	std::string shaderCode;

	FileSystem::readFile(path, shaderCode);

	return shaderCode;
}

unsigned ShaderBase::getID() const {
	return ID;
}

void ShaderBase::use() const {
	SHADER_CONTROLLER->useShader(getID());
}

void ShaderBase::setInt(const char* name, int val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform1i(getUniformLocation(name), val);
}

void ShaderBase::setBool(const char* name, bool val) {
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

void ShaderBase::setMat3(const char* name, const glm::mat3& val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(val));
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

void ShaderBase::setVec4(const char* name, const glm::vec4& val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(val));
}

void ShaderBase::setFloat(const char* name, float val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniform1f(getUniformLocation(name), val);
}

void ShaderBase::setUniformBlockIdx(const char* name, unsigned val) {
	if (getUniformLocation(name) == -1) {
		return;
	}
	glUniformBlockBinding(getUniformLocation(name), val, 0);
}

ShaderBase::~ShaderBase() {
	SHADER_CONTROLLER->deleteShaderGL(ID);
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

int ShaderBase::getUniformLocation(const std::string& name) {
	const auto found = cachedUniforms.find(name);
	if (found != cachedUniforms.end()) {
		return found->second;
	}

	return cachedUniforms.insert({ name, glGetUniformLocation(ID, name.c_str()) }).first->second;
}