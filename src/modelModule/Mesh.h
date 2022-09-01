#pragma once
#include <string>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vector>

#include "glad/glad.h"
#include "renderModule/TextureHandler.h"
#include "shaderModule/Shader.h"


namespace GameEngine {
	namespace FrustumModule {
		struct AABB;
	}
}

namespace GameEngine::ModelModule {
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Tangent;
	};

	struct MaterialTexture {
		RenderModule::Texture mTexture;
		std::string mType;
	};

	struct Material {
		MaterialTexture mDiffuse;
		MaterialTexture mNormal;
		MaterialTexture mSpecular;
	};

	class Mesh {
	public:
		// mesh data
		std::vector<Vertex> mVertices;
		std::vector<unsigned int> mIndices;
		Mesh() = default;
		void setupMesh();

		~Mesh();
		Mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
		
		unsigned int getVAO() const { return VAO;}
		void draw(ShaderModule::ShaderBase* shader, bool ignoreTex = false);

		FrustumModule::AABB* bounds = nullptr;
		Material mMaterial;
	private:
		
		//  render data
		unsigned int VAO = -1, VBO = -1, EBO = -1;
		const GLuint type = GL_TRIANGLES;
	};
}
