#include "GeometryShader.h"

#include "glWrapper/Shader.h"
#include "logsModule/logger.h"

using namespace SFE::ShaderModule;

bool GeometryShader::compile() {
	id = GLW::createProgram();
	
	const char* errorStr;

	auto success = true;
	success |= GLW::compileShader(loadShaderCode(vertexPath.c_str()).c_str(), GLW::ShaderType::VERTEX, id, errorStr);
	success |= GLW::compileShader(loadShaderCode(fragmentPath.c_str()).c_str(), GLW::ShaderType::FRAGMENT, id, errorStr);
	success |= GLW::compileShader(loadShaderCode(geometryPath.c_str()).c_str(), GLW::ShaderType::GEOMETRY, id, errorStr);

	if (!success){
		LogsModule::Logger::LOG_ERROR("[%s, %s, %s] error downloading\n%s", vertexPath.c_str(), fragmentPath.c_str(), geometryPath.c_str(), errorStr);
	}
	return success;
}

std::string_view GeometryShader::getVertexPath() {
	return vertexPath;
}

std::string_view GeometryShader::getFragmentPath() {
	return fragmentPath;
}

std::string_view GeometryShader::getGeometryPath() {
	return geometryPath;
}

GeometryShader::GeometryShader(const char* vsPath, const char* fsPath, const char* gsPath, size_t hash) : ShaderBase(hash), vertexPath(vsPath), fragmentPath(fsPath), geometryPath(gsPath) {
	
}
