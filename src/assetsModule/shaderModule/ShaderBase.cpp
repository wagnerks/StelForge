#include "ShaderBase.h"

#include "ShaderController.h"
#include "core/FileSystem.h"
#include "glWrapper/Shader.h"
#include "logsModule/logger.h"

using namespace SFE::ShaderModule;

std::string ShaderBase::loadShaderCode(std::string_view path) {

	std::string shaderCode;

	FileSystem::readFile(path, shaderCode);

	return shaderCode;
}

unsigned ShaderBase::getID() const {
	return id;
}

void ShaderBase::use() const {
	SHADER_CONTROLLER->useShader(getID());
}

ShaderBase::~ShaderBase() {
	SHADER_CONTROLLER->deleteShaderGL(id);
}

