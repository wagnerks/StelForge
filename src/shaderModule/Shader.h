#pragma once
#include <string>
#include <unordered_map>

#include "ShaderBase.h"


namespace Engine::ShaderModule {
	

	class Shader : public ShaderBase {
		friend class ShaderController;
	public:
		Shader(const Shader& other) = delete;
		Shader(Shader&& other) noexcept = delete;
		Shader& operator=(const Shader& other) = delete;
		Shader& operator=(Shader&& other) noexcept = delete;

		bool compile() override;
		std::string_view getVertexPath();
		std::string_view getFragmentPath();
	protected:
		Shader() = default;
		Shader(const char* vertexPath, const char* fragmentPath, size_t hash);
	private:
		std::string vertexPath;
		std::string fragmentPath;
	};
}
