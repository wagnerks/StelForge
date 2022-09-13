#include "Mesh.h"

#include "core/BoundingVolume.h"
#include "renderModule/Renderer.h"

using namespace GameEngine::ModelModule;

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<unsigned>& indices) {
	mData.mVertices = std::move(vertices);
	mData.mIndices = std::move(indices);

	bindMesh();
}

Mesh::Mesh(Mesh&& other) noexcept: mBounds(other.mBounds),
                                   mMaterial(std::move(other.mMaterial)),
                                   mData(std::move(other.mData)),
                                   mBinded(other.mBinded) {
	other.mBinded = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	mBounds = other.mBounds;
	mMaterial = std::move(other.mMaterial);
	mData = std::move(other.mData);
	mBinded = other.mBinded;

	other.mBinded = false;

	return *this;
}

Mesh::~Mesh() {
	unbindMesh();
}

void Mesh::bindMesh() {
	unbindMesh();
	if (mBinded) {
		return;
	}
	mBinded = true;

    glGenVertexArrays(1, &mData.mVao);
    glGenBuffers(1, &mData.mVbo);
    glGenBuffers(1, &mData.mEbo);
  
    glBindVertexArray(mData.mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mData.mVbo);

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizei>(mData.mVertices.size() * sizeof(Vertex)), mData.mVertices.data(), GL_STATIC_DRAW);  

	if (!mData.mIndices.empty()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mData.mEbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizei>(mData.mIndices.size() * sizeof(unsigned int)), mData.mIndices.data(), GL_STATIC_DRAW);
	}

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), static_cast<void*>(0));
    // vertex normals
    glEnableVertexAttribArray(1);	
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, mNormal)));
    // vertex texture coords
    glEnableVertexAttribArray(2);	
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, mTexCoords)));
	// vertex tangent
    glEnableVertexAttribArray(3);	
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, mTangent)));

    glBindVertexArray(0);

	auto minAABB = glm::vec3(std::numeric_limits<float>::max());
	auto maxAABB = glm::vec3(std::numeric_limits<float>::min());

	for (auto& vertex : mData.mVertices) {
		minAABB.x = std::min(minAABB.x, vertex.mPosition.x);
		minAABB.y = std::min(minAABB.y, vertex.mPosition.y);
		minAABB.z = std::min(minAABB.z, vertex.mPosition.z);

		maxAABB.x = std::max(maxAABB.x, vertex.mPosition.x);
		maxAABB.y = std::max(maxAABB.y, vertex.mPosition.y);
		maxAABB.z = std::max(maxAABB.z, vertex.mPosition.z);
	}
	
	mBounds = new FrustumModule::AABB(minAABB, maxAABB);
}

void Mesh::unbindMesh() {
	if (!mBinded) {
		return;
	}
	mBinded = false;

	glDeleteVertexArrays(1, &mData.mVao);
	glDeleteBuffers(1, &mData.mVbo);
	glDeleteBuffers(1, &mData.mEbo);

	mData.mVao = std::numeric_limits<unsigned>::max();
	mData.mVbo = std::numeric_limits<unsigned>::max();
	mData.mEbo = std::numeric_limits<unsigned>::max();
}
