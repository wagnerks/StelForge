#include "ShaderBase.h"

#include "core/FileSystem.h"

using namespace SFE::ShaderModule;

std::string ShaderBase::loadShaderCode(std::string_view path) {

	std::string shaderCode;

	FileSystem::readFile(path, shaderCode);

	return shaderCode;
}
