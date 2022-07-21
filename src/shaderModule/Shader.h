#pragma once
#include <fwd.hpp>
#include <string>
#include <unordered_map>


namespace GameEngine::ShaderModule {
	class Shader {
		friend class ShaderController;
	public:
		void use() const;
		unsigned int getID() const;

		void setInt(const char* name, int val);
		void setMat4(const char* name, const glm::mat4& val);
		void setVec2(const char* name, const glm::vec2& val);
		void setVec3(const char* name, const glm::vec3& val);

		Shader(const Shader& other) = delete;
		Shader(Shader&& other) noexcept = delete;
		Shader& operator=(const Shader& other) = delete;
		Shader& operator=(Shader&& other) noexcept = delete;
		
	protected:
		bool compileShader(const char* vShaderCode, const char* fShaderCode);
		static std::pair<std::string, std::string> loadShaderCode(const char* vertexPath, const char* fragmentPath);
		size_t hash = 0;

		~Shader() = default;
		Shader() = default;
		Shader(const char* vertexPath, const char* fragmentPath, size_t hash);
	private:
		unsigned int ID = -1;
		

		bool checkCompileErrors(unsigned int shader, std::string_view type);
		int getUniformLocation(const char* name);
		
		std::unordered_map<const char*, int> cachedUniforms;
	};
}
