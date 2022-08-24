#include "GeometryShader.h"

#include "glad/glad.h"
#include "logsModule/logger.h"

using namespace GameEngine::ShaderModule;

bool GeometryShader::compile() {
	cachedUniforms.clear();
	ID = glCreateProgram();
	auto success = true;
	success = compileShader(loadShaderCode(vertexPath.c_str()).c_str(), GL_VERTEX_SHADER) && success;
	success = compileShader(loadShaderCode(fragmentPath.c_str()).c_str(), GL_FRAGMENT_SHADER) && success;
	success = compileShader(loadShaderCode(geometryPath.c_str()).c_str(), GL_GEOMETRY_SHADER) && success;
	if (!success){
		LogsModule::Logger::LOG_ERROR("[%s, %s, %s] error downloading", vertexPath.c_str(), fragmentPath.c_str(), geometryPath.c_str());
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
