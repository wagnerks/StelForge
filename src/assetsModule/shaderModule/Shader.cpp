#include "Shader.h"
#include "glWrapper/Shader.h"

#include <filesystem>
#include <map>
#include <string>
#include "logsModule/logger.h"

using namespace SFE;
using namespace SFE::ShaderModule;


Shader::Shader(const char* vertexPath, const char* fragmentPath, size_t hash) : ShaderBase(hash), vertexPath(vertexPath), fragmentPath(fragmentPath) {
}

bool Shader::compile() {
	id = GLW::createProgram();

	vertexCode = loadShaderCode(vertexPath);
	fragmentCode = loadShaderCode(fragmentPath);
	const char* errorStr;
	auto success = GLW::compileShader(vertexCode.c_str(), GLW::ShaderType::VERTEX, id, errorStr);
	success |= GLW::compileShader(fragmentCode.c_str(), GLW::ShaderType::FRAGMENT, id, errorStr);
	if (!success) {
		LogsModule::Logger::LOG_ERROR("[%s, %s] error downloading\n%s", vertexPath.c_str(), fragmentPath.c_str(), errorStr);
	}

	return success;
}

std::string_view Shader::getVertexPath() {
	return vertexPath;
}

std::string_view Shader::getFragmentPath() {
	return fragmentPath;
}


