#include "Mesh.h"

#include "core/BoundingVolume.h"
#include "renderModule/Renderer.h"
#include "renderModule/TextureHandler.h"


using namespace GameEngine::ModelModule;

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<unsigned>& indices) :
	mVertices(std::move(vertices)), mIndices(std::move(indices)) {
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
    glBindVertexArray(VAO);
	if (!mIndices.empty()) {
		RenderModule::Renderer::drawElements(type, static_cast<GLsizei>(mIndices.size()), GL_UNSIGNED_INT);
	}
	else {
		RenderModule::Renderer::drawArrays(type, static_cast<int>(mVertices.size()));
	}

    glBindVertexArray(0);
}

void  Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
  
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(mVertices.size() * sizeof(Vertex)), mVertices.data(), GL_STATIC_DRAW);  

	if (!mIndices.empty()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(mIndices.size() * sizeof(unsigned int)), mIndices.data(), GL_STATIC_DRAW);
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

		for (auto& vertex : mVertices)
		{
			minAABB.x = std::min(minAABB.x, vertex.Position.x);
			minAABB.y = std::min(minAABB.y, vertex.Position.y);
			minAABB.z = std::min(minAABB.z, vertex.Position.z);

			maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
			maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
			maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
		}
		


	bounds = new FrustumModule::AABB(minAABB, maxAABB);
}