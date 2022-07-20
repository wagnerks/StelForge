#pragma once

namespace GameEngine::Render {
	class IndexBuffer {
	public:
		IndexBuffer();
		~IndexBuffer();

		void bindIndexData(const unsigned int* data, size_t count);
		void bind() const;
		void unbind() const;

		size_t getCount() const;
	private:
		size_t count = 0;
		unsigned int rendererID = 0;
	};
}
