#pragma once

#include "Shader.h"
#include "core/Singleton.h"

#define SHADER_CONTROLLER ::Engine::ShaderModule::ShaderController::instance()

namespace Engine::ShaderModule {
	class ShaderController : public Singleton<ShaderController> {
		friend Singleton;
	public:
		ShaderController();
		~ShaderController();
		void init() override;
		ShaderBase* loadVertexFragmentShader(const std::string& vertexPath, const std::string& fragmentPath);
		ShaderBase* loadGeometryShader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath);
		void recompileShader(ShaderBase* shader);

		void initDefaultShader();
		void useShader(unsigned int ID);
		void useDefaultShader();
		void deleteShaderGL(unsigned int ID);
		void deleteShader(ShaderBase* shader);
		void deleteShader(size_t shaderHash);

		ShaderBase* defaultShader = nullptr;
		void removeShader(ShaderBase* shader);
		const std::unordered_map<size_t, ShaderBase*>& getShaders();
		ShaderBase* getShader(size_t shaderID);

	private:
		std::unordered_map<size_t, ShaderBase*> shaders;
		std::hash<std::string> hasher;

		unsigned int currentShader = 0;
	};
}

