#pragma once

namespace GameEngine::Render {
	class VertexBuffer {
	public:
		VertexBuffer();
		~VertexBuffer();
		void bindBufferData(const void* data, unsigned int size) const;

		void bind() const;
		void unbind() const;
		void clear();
	private:
		unsigned int rendererID = 0;
		
	};
}
