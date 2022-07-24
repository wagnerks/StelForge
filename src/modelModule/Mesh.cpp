﻿#include "Mesh.h"


using namespace GameEngine::ModelModule;

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<unsigned>& indices, std::vector<MeshTexture>& textures) :
	vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)) {
	setupMesh();
}

void Mesh::Draw(ShaderModule::Shader* shader) {
	unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
	for (unsigned int i = 0; i < textures.size(); i++) {
		glActiveTexture(GL_TEXTURE0 + i); // activate proper texture unit before binding
		// retrieve texture number (the N in diffuse_textureN)
		std::string number;
		std::string name = textures[i].type;
		if (name == "texture_diffuse")
			number = std::to_string(diffuseNr++);
		else if (name == "texture_specular")
			number = std::to_string(specularNr++);

		shader->setInt((name + number).c_str(), i);
		glBindTexture(GL_TEXTURE_2D, textures[i].id);
	}
    glActiveTexture(GL_TEXTURE0);


    glBindVertexArray(VAO);
	if (!indices.empty()) {
		 glDrawElements(type, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
	}
	else {
		glDrawArrays(type, 0, static_cast<int>(vertices.size()));
	}
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // vertex normals
    glEnableVertexAttribArray(1);	
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    // vertex texture coords
    glEnableVertexAttribArray(2);	
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
}