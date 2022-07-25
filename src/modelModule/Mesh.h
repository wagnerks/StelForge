#pragma once
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "glad/glad.h"
#include "shaderModule/Shader.h"


namespace GameEngine::ModelModule {
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
	};

	struct MeshTexture {
		unsigned int id = 0;
		std::string type;
	};

	class Mesh {
	public:
		// mesh data
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<MeshTexture> textures;

		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, std::vector<MeshTexture>& textures);
		~Mesh();
		unsigned int getVAO() const { return VAO;}
		void draw(ShaderModule::ShaderBase* shader);
	private:
		//  render data
		unsigned int VAO = -1, VBO = -1, EBO = -1;
		const GLuint type = GL_TRIANGLES;
		void setupMesh();
	};
}
