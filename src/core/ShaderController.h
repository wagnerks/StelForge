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
		static ShaderController* getInstance();
		static void terminate();

		void initDefaultShader();
		void useShader(unsigned int ID);
		void useDefaultShader();
		void deleteShader(unsigned int ID);

		Shader* defaultShader = nullptr;
		void removeShader(const std::string& hash);
		const std::unordered_map<std::string, Shader*>& getShaders();
	private:
		std::unordered_map<std::string, Shader*> shaders;

		GLuint currentShader = 0;

		inline static ShaderController* instance = nullptr;
		
	};
}

