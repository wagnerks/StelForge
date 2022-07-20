﻿#include "shader.h"

#include <filesystem>
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <gtc/type_ptr.hpp>

#include "logger.h"
#include "ShaderController.h"

using namespace GameEngine;
using namespace GameEngine::Render;


Shader::Shader(const char* vertexPath, const char* fragmentPath) : path(vertexPath + std::string(fragmentPath)) {
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try {
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;

		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		vShaderFile.close();
		fShaderFile.close();

		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure& e) {
		Logger::LOG_ERROR("SHADER::FILE_NOT_SUCCESSFULLY_READ: %s", e.what());
		return;
	}

	compileShader();
}

unsigned Shader::getID() const {
	return ID;
}

void Shader::use() const {
	ShaderController::getInstance()->useShader(getID());
}

void Shader::setInt(const char* name, int val) {
	glUniform1i(getUniformLocation(name), val);
}

void Shader::setMat4(const char* name, const glm::mat4& val) {
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void Shader::setVec2(const char* name, const glm::vec2& val) {
	glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(val));
}

void Shader::setVec3(const char* name, const glm::vec3& val) {
	glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(val));
}

bool Shader::checkCompileErrors(unsigned int shader, std::string_view type) {
	int success;
	char infoLog[1024];
	if (type != "PROGRAM") {
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);

			Logger::LOG_ERROR("SHADER_COMPILATION_ERROR of type: %s\n%s\n-- --------------------------------------------------- --", type.data(), infoLog);
		}
	}
	else {
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);

			Logger::LOG_ERROR("PROGRAM_LINKING_ERROR of type: %s\n%s\n-- --------------------------------------------------- --", type.data(), infoLog);
		}
	}

	return success;
}

int Shader::getUniformLocation(const char* name) {
	const auto found = cachedUniforms.find(name);
	if (found != cachedUniforms.end()) {
		return found->second;
	}

	const auto loc = glGetUniformLocation(ID, name);
	cachedUniforms[name] = loc;

	return loc;
}

bool Shader::compileShader() {
	ShaderController::getInstance()->deleteShader(getID());
	cachedUniforms.clear();

	const auto vShaderCode = vertexCode.c_str();
	const auto fShaderCode = fragmentCode.c_str();

	const auto vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, nullptr);
	glCompileShader(vertex);

	auto success = checkCompileErrors(vertex, "VERTEX");

	const auto fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, nullptr);
	glCompileShader(fragment);
	success = success || checkCompileErrors(fragment, "FRAGMENT");

	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	glAttachShader(ID, fragment);

	glLinkProgram(ID);
	success = success || checkCompileErrors(ID, "PROGRAM");

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	return success;
}