#include "Mesh.h"

#include "core/BoundingVolume.h"
#include "renderModule/Renderer.h"
#include "renderModule/TextureHandler.h"


using namespace GameEngine::ModelModule;

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<unsigned>& indices, std::vector<MeshTexture>& textures) :
	vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)) {
	setupMesh();
}

Mesh::~Mesh() {
	if (VAO != -1) {
		glDeleteVertexArrays(1, &VAO);
	}

	if (VBO != -1) {
		glDeleteBuffers(1, &VBO);
	}

	if (EBO != -1) {
		glDeleteBuffers(1, &EBO);
	}
}

void Mesh::draw(ShaderModule::ShaderBase* shader, bool ignoreTex) {
	if (!ignoreTex) {
		unsigned int diffuseNr = 1;
	    unsigned int specularNr = 1;
		for (unsigned int i = 0; i < textures.size(); i++) {
			std::string number;
			std::string name = textures[i].type;
			if (name == "texture_diffuse")
				number = std::to_string(diffuseNr++);
			else if (name == "texture_specular")
				number = std::to_string(specularNr++);

			shader->setInt((name + number).c_str(), i);

			RenderModule::TextureHandler::getInstance()->bindTexture(GL_TEXTURE0 + i, GL_TEXTURE_2D, textures[i].id);
		}
	}
	
	
    glBindVertexArray(VAO);
	if (!indices.empty()) {
		RenderModule::Renderer::drawElements(type, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT);
	}
	else {
		RenderModule::Renderer::drawArrays(type, static_cast<int>(vertices.size()));
	}

    glBindVertexArray(0);
}

void  Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
  
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);  

	if (!indices.empty()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
	}

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), static_cast<void*>(0));
    // vertex normals
    glEnableVertexAttribArray(1);	
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, Normal)));
    // vertex texture coords
    glEnableVertexAttribArray(2);	
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, TexCoords)));
	// vertex tangent
    glEnableVertexAttribArray(3);	
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, Tangent)));

    glBindVertexArray(0);

	glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::min());

		for (auto& vertex : vertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.Position.x);
			minAABB.y = std::min(minAABB.y, vertex.Position.y);
			minAABB.z = std::min(minAABB.z, vertex.Position.z);

			maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
			maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
			maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
		}
		


	bounds = new FrustumModule::Sphere((maxAABB + minAABB) * 0.5f, glm::length(minAABB - maxAABB));
}