#pragma once
#include <vector>
#include <glad/glad.h>

namespace GameEngine::Render {
	struct VertexBufferElement {
		unsigned int type;
		unsigned int count;
		unsigned char normalized;
	};

	class VertexBufferLayout {
	public:
		inline static unsigned int getTypeSize(unsigned int type) {
			switch(type) {
				case GL_FLOAT:			return 4;
				case GL_UNSIGNED_INT:	return 4;
				case GL_UNSIGNED_BYTE:	return 1;
			}
			return 0;
		};

		VertexBufferLayout() : stride(0){};

		template <typename T>
		void push(unsigned int count) {
		}

		template<>
		void push<float>(unsigned int count) {
			elements.emplace_back(GL_FLOAT, count, GL_FALSE);
			stride += count * getTypeSize(GL_FLOAT);
		}

		template<>
		void push<unsigned>(unsigned int count) {
			elements.emplace_back(GL_UNSIGNED_INT, count, GL_FALSE);
			stride += count * getTypeSize(GL_UNSIGNED_INT);
		}

		template<>
		void push<unsigned char>(unsigned int count) {
			elements.emplace_back(GL_UNSIGNED_BYTE, count, GL_TRUE);
			stride += count * getTypeSize(GL_UNSIGNED_BYTE);
		}

		inline const std::vector<VertexBufferElement>& getElements() const { return elements;};
		inline unsigned int getStride() const {return stride;}
	private:
		std::vector<VertexBufferElement> elements;
		unsigned int stride;
	};
}