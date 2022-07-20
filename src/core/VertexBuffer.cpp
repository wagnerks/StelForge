#include "VertexBuffer.h"

#include "glad/glad.h"

using namespace GameEngine::Render;

VertexBuffer::VertexBuffer() {
	glGenBuffers(1, &rendererID);
}

VertexBuffer::~VertexBuffer() {
	glDeleteBuffers(1, &rendererID);
}

void VertexBuffer::bindBufferData(const void* data, unsigned size) const {
	bind();
	glBufferData(GL_ARRAY_BUFFER, size,  data, GL_STATIC_DRAW);
}

void VertexBuffer::bind() const {
	glBindBuffer(GL_ARRAY_BUFFER, rendererID);
}

void VertexBuffer::unbind() const {
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::clear() {
	glDeleteBuffers(1, &rendererID);
	glGenBuffers(1, &rendererID);
}
