#pragma once
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "glad/glad.h"
#include "shaderModule/Shader.h"


namespace GameEngine {
	namespace FrustumModule {
		struct Sphere;
	}
}

namespace GameEngine::ModelModule {
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Tangent;
	};
	
	class Mesh {
	public:
		// mesh data
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		Mesh() = default;
		void setupMesh();

		~Mesh();
		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
		
		unsigned int getVAO() const { return VAO;}
		void draw(ShaderModule::ShaderBase* shader, bool ignoreTex = false);

		FrustumModule::Sphere* bounds = nullptr;
		
	private:
		
		//  render data
		unsigned int VAO = -1, VBO = -1, EBO = -1;
		const GLuint type = GL_TRIANGLES;
	};
}
