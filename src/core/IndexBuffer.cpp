#include "IndexBuffer.h"

#include "glad/glad.h"

using namespace GameEngine::Render;

IndexBuffer::IndexBuffer() {
	glGenBuffers(1, &rendererID);
	
}

IndexBuffer::~IndexBuffer() {
	glDeleteBuffers(1, &rendererID);
}

void IndexBuffer::bindIndexData(const unsigned* data, size_t count) {
	this->count = count;
	bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int),  data, GL_STATIC_DRAW);
}

void IndexBuffer::bind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rendererID);
}

void IndexBuffer::unbind() const {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

size_t IndexBuffer::getCount() const {
	return count;
}
