#pragma once
#include "Shader.h"

namespace GameEngine::ShaderModule {
	class GeometryShader : public ShaderBase {
		friend class ShaderController;
	public:
		GeometryShader(const GeometryShader& other) = delete;
		GeometryShader(GeometryShader&& other) noexcept = delete;
		GeometryShader& operator=(const GeometryShader& other) = delete;
		GeometryShader& operator=(GeometryShader&& other) noexcept = delete;
		bool compile() override;
		
		std::string_view getVertexPath();
		std::string_view getFragmentPath();
		std::string_view getGeometryPath();
	protected:
		GeometryShader() = default;
		GeometryShader(const char* vsPath, const char* fsPath, const char* gsPath, size_t hash);
	private:
		std::string vertexPath;
		std::string fragmentPath;
		std::string geometryPath;
	};
}
