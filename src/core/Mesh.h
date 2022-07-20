#pragma once
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>
#include "glad/glad.h"



namespace GameEngine::Render {
	class Shader;

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
	};

	struct MeshTexture {
		unsigned int id = 0;
		std::string type;
		std::string path;
	};

	class Mesh {
	public:
		// mesh data
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<MeshTexture> textures;

		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, std::vector<MeshTexture>& textures);

		void Draw(Shader* shader);
	private:
		//  render data
		unsigned int VAO = 0, VBO = 0, EBO = 0;
		const GLuint type = GL_TRIANGLES;
		void setupMesh();
	};
}
