#pragma once
#include <fwd.hpp>
#include <string>
#include <unordered_map>

namespace Engine::ShaderModule {
	class ShaderBase {
		friend class ShaderController;
	public:
		static std::string loadShaderCode(std::string_view path);

		virtual bool compile() = 0;
		void use() const;
		unsigned int getID() const;

		template <typename T>
		void setValue(std::string_view, const T&) {}

		void setInt(const char* name, int val);
		void setBool(const char* name, bool val);
		void setMat4(const char* name, const glm::mat4& val);
		void setMat3(const char* name, const glm::mat3& val);
		void setVec2(const char* name, const glm::vec2& val);
		void setVec3(const char* name, const glm::vec3& val);
		void setVec4(const char* name, const glm::vec4& val);
		void setFloat(const char* name, float val);
		void setUniformBlockIdx(const char* name, unsigned val);

		ShaderBase(const ShaderBase& other) = delete;
		ShaderBase(ShaderBase&& other) noexcept = delete;
		ShaderBase& operator=(const ShaderBase& other) = delete;
		ShaderBase& operator=(ShaderBase&& other) noexcept = delete;

		std::string vertexCode;
		std::string fragmentCode;

		bool compileShader(const char* shaderCode, unsigned type);

	protected:
		virtual ~ShaderBase();
		ShaderBase() = default;
		ShaderBase(size_t hash) : hash(hash) {};
		unsigned int ID = 0;
		std::unordered_map<std::string, int> cachedUniforms;


	private:
		size_t hash = 0;

		static bool checkCompileErrors(unsigned int shader, std::string_view type);
		int getUniformLocation(const std::string& name);

	};



	template <>
	inline void ShaderBase::setValue<int>(std::string_view name, const int& val) {
		setInt(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<float>(std::string_view name, const float& val) {
		setFloat(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<glm::mat4>(std::string_view name, const glm::mat4& val) {
		setMat4(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<glm::mat3>(std::string_view name, const glm::mat3& val) {
		setMat3(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<glm::vec4>(std::string_view name, const glm::vec4& val) {
		setVec4(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<glm::vec3>(std::string_view name, const glm::vec3& val) {
		setVec3(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<glm::vec2>(std::string_view name, const glm::vec2& val) {
		setVec2(name.data(), val);
	}
}
