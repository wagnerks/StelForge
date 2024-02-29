#pragma once

#include "glad/glad.h"
#include "unordered_map"

namespace SFE::GLW {
	enum class ShaderType : GLenum {
		NONE,
		COMPUTE = GL_COMPUTE_SHADER,
		VERTEX = GL_VERTEX_SHADER,
		TESS_CONTROL = GL_TESS_CONTROL_SHADER,
		TESS_EVALUATION = GL_TESS_EVALUATION_SHADER,
		GEOMETRY = GL_GEOMETRY_SHADER,
		FRAGMENT = GL_FRAGMENT_SHADER
	};

	inline static std::unordered_map<int, std::unordered_map<std::string, int>> cachedUniforms;

	inline int createProgram() {
		const auto id = glCreateProgram();
		cachedUniforms[id].clear();
		return id;
	}

	inline void deleteProgram(unsigned int programId) {
		cachedUniforms.erase(programId);
		glDeleteProgram(programId);
	}

	constexpr void useProgram(unsigned int programId) {
		glUseProgram(programId);
	}

	constexpr bool checkCompileErrors(unsigned int shader, const char*& result) {
		int success;
		char infoLog[1024];
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
			result = infoLog;
		}

		return success;
	}

	constexpr bool compileShader(const char* shaderCode, ShaderType type, unsigned shaderId, const char*& res) {
		const auto shader = glCreateShader(static_cast<GLenum>(type));
		glShaderSource(shader, 1, &shaderCode, nullptr);
		glCompileShader(shader);

		glAttachShader(shaderId, shader);

		glLinkProgram(shaderId);
		const auto success = checkCompileErrors(shaderId, res);
		glDeleteShader(shader);

		return success;
	}

	inline int getUniformLocation(const std::string& name, int shaderId) {
		auto& shader = cachedUniforms[shaderId];

		const auto found = shader.find(name);
		if (found != shader.end()) {
			return found->second;
		}

		return shader.insert({ name, glGetUniformLocation(shaderId, name.c_str()) }).first->second;
	}

	inline void setUniformBlockIdx(int shaderId, const std::string& name, unsigned val) {
		const auto loc = GLW::getUniformLocation(name, shaderId);
		if (loc == -1) {
			return;
		}

		glUniformBlockBinding(loc, val, 0);
	}

	template<typename Type>
	constexpr void setUniformValue(int /*uniformLocation*/, Type /*value*/) {}

	template<typename Type>
	constexpr void setUniform2Value(int /*uniformLocation*/, Type* /*value*/, int count) {}

	template<typename Type>
	constexpr void setUniform3Value(int /*uniformLocation*/, Type* /*value*/, int count) {}

	template<typename Type>
	constexpr void setUniform4Value(int /*uniformLocation*/, Type* /*value*/, int count) {}

	template<typename Type>
	constexpr void setUniformMat3Value(int /*uniformLocation*/, Type* /*value*/, bool transpose, int count) {}

	template<typename Type>
	constexpr void setUniformMat4Value(int /*uniformLocation*/, Type* /*value*/, bool transpose, int count) {}

	template<typename Type>
	inline void setUniformValue(int shaderId, const std::string& uniformName, const Type& value) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniformValue(loc, value);
	}

	template<typename Type>
	inline void setUniform2Value(int shaderId, const std::string& uniformName, const Type* value, int count = 1) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniform2Value(loc, value, count);
	}

	template<typename Type>
	inline void setUniform3Value(int shaderId, const std::string& uniformName, const Type* value, int count = 1) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniform3Value(loc, value, count);
	}

	template<typename Type>
	inline void setUniform4Value(int shaderId, const std::string& uniformName, const Type* value, int count = 1) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniform4Value(loc, value, count);
	}

	template<typename Type>
	inline void setUniformMat3Value(int shaderId, const std::string& uniformName, const Type* value, bool transpose = false, int count = 1) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniformMat3Value(loc, value, transpose, count);
	}

	template<typename Type>
	inline void setUniformMat4Value(int shaderId, const std::string& uniformName, const Type* value, bool transpose = false, int count = 1) {
		auto loc = GLW::getUniformLocation(uniformName, shaderId);
		if (loc == -1) {
			return;
		}

		setUniformMat4Value(loc, value, transpose, count);
	}

	template<>
	constexpr void setUniformValue(int uniformLocation, float value) {
		glUniform1f(uniformLocation, value);
	}

	template<>
	constexpr void setUniformValue(int uniformLocation, int value) {
		glUniform1i(uniformLocation, value);
	}

	template<>
	constexpr void setUniformValue(int uniformLocation, bool value) {
		glUniform1i(uniformLocation, value);
	}

	template<>
	constexpr void setUniform2Value(int uniformLocation, const float* value, int count) {
		glUniform2fv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniform2Value(int uniformLocation, const int* value, int count) {
		glUniform2iv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniform3Value(int uniformLocation, const float* value, int count) {
		glUniform3fv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniform3Value(int uniformLocation, const int* value, int count) {
		glUniform3iv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniform4Value(int uniformLocation, const float* value, int count) {
		glUniform4fv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniform4Value(int uniformLocation, const int* value, int count) {
		glUniform4iv(uniformLocation, count, value);
	}

	template<>
	constexpr void setUniformMat3Value(int uniformLocation, const float* value, bool transpose, int count) {
		glUniformMatrix3fv(uniformLocation, count, transpose, value);
	}

	template<>
	constexpr void setUniformMat4Value(int uniformLocation, const float* value, bool transpose, int count) {
		glUniformMatrix4fv(uniformLocation, count, transpose, value);
	}
}
