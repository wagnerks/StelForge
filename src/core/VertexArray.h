#pragma once

namespace GameEngine::Render {
	class VertexBuffer;
	class VertexBufferLayout;

	class VertexArray {
	public:
		VertexArray();
		~VertexArray();

		void addBuffer(const VertexBuffer* vertexBuffer, const VertexBufferLayout* layout) const;
		void bind() const;
		void unbind() const;
	private:
		unsigned int renderID;
	};

}