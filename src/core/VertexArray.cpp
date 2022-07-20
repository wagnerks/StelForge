#include "VertexArray.h"

#include "VertexBuffer.h"
#include "VertexBufferLayout.h"
#include "glad/glad.h"

using namespace GameEngine::Render;

VertexArray::VertexArray() : renderID(0) {
	glGenVertexArrays(1, &renderID);
}

VertexArray::~VertexArray() {
	glDeleteVertexArrays(1, &renderID);
}

void VertexArray::addBuffer(const VertexBuffer* vertexBuffer, const VertexBufferLayout* layout) const {
	bind();

	vertexBuffer->bind();

	const auto& elements = layout->getElements();
	size_t offset = 0;
	for (auto i = 0u; i < elements.size(); i++) {
		const auto& element = elements[i];
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, element.count, element.type, element.normalized, layout->getStride(), (void*)offset);

		offset += element.count * VertexBufferLayout::getTypeSize(element.type);
	}
}

void VertexArray::bind() const {
	glBindVertexArray(renderID);
}

void VertexArray::unbind() const {
	glBindVertexArray(0);
}
