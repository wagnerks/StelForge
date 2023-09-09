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

		void setInt(const char* name, int val);
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
	protected:
		virtual ~ShaderBase();
		ShaderBase() = default;
		ShaderBase(size_t hash) : hash(hash) {};
		unsigned int ID = 0;
		std::unordered_map<std::string, int> cachedUniforms;

		bool compileShader(const char* shaderCode, unsigned type);
	private:
		size_t hash = 0;

		static bool checkCompileErrors(unsigned int shader, std::string_view type);
		int getUniformLocation(const std::string& name);

	};
}
