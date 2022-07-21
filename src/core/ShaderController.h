#pragma once

#include "Renderer.h"
#include "shader.h"

#define SHADER_CONTROLLER GameEngine::Render::ShaderController::getInstance()

namespace GameEngine::Render {
	class ShaderController {
	public:
		ShaderController();
		~ShaderController();
		void init();
		Shader* loadShader(const std::string& vertexPath, const std::string& fragmentPath);
		void recompileShader(Shader* shader);
		static ShaderController* getInstance();
		static void terminate();

		void initDefaultShader();
		void useShader(unsigned int ID);
		void useDefaultShader();
		void deleteShaderGL(unsigned int ID);

		Shader* defaultShader = nullptr;
		void removeShader(Shader* shader);
		const std::unordered_map<size_t, Shader*>& getShaders();
		const std::unordered_map<size_t, std::pair<std::string, std::string>>& getShaderPaths();
	private:
		std::unordered_map<size_t, Shader*> shaders;
		std::unordered_map<size_t, std::pair<std::string, std::string>> shaderPaths;
		std::hash<std::string> hasher;

		GLuint currentShader = 0;

		inline static ShaderController* instance = nullptr;
		
	};
}

