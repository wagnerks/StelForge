#pragma once

#include <cstddef>
#include <glad/glad.h>

namespace SFE::Render {

	enum AttributeFType {
		FLOAT = GL_FLOAT,
		DOUBLE = GL_DOUBLE
	};

	enum AttributeIType {
		BYTE = GL_BYTE,
		UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
		SHORT = GL_SHORT,
		UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
		INT = GL_INT,
		UNSIGNED_INT = GL_UNSIGNED_INT
	};

	template<size_t Count = 1>
	class VertexArrays {
	public:
		VertexArrays() {}
		
		~VertexArrays() {
			glDeleteVertexArrays(Count, mVaoId);
		}

		void bind(size_t index = 0) const {
			glBindVertexArray(mVaoId[index]);
		}

		void generate() {
			glGenVertexArrays(Count, mVaoId);
		}

		void release() {
			glDeleteVertexArrays(Count, mVaoId);
		}

		static void bindArray(unsigned vaoId) {
			glBindVertexArray(vaoId);
		}
		static void bindDefault() {
			glBindVertexArray(0);
		}

		template <typename T>
		void addAttribute(unsigned index, int size, AttributeFType type, bool normalized, size_t offset = 0) {
			addAttribute(index, size, type, normalized, sizeof(T), offset);
		}

		template <typename T, typename Member>
		void addAttribute(unsigned index, int size, AttributeFType type, bool normalized, Member T::* memberPtr) {
			addAttribute(index, size, type, normalized, sizeof(T), (reinterpret_cast<size_t>(&((T*)nullptr->*memberPtr))));
		}

		void addAttribute(unsigned index, int size, AttributeFType type, bool normalized, int stride, size_t offset = 0) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<void*>(offset));
		}

		template <typename T>
		void addAttribute(unsigned index, int size, AttributeIType type) {
			addAttribute(index, size, type, sizeof(T), 0);
		}

		template <typename T, typename Member>
		void addAttribute(unsigned index, int size, AttributeIType type, Member T::* memberPtr) {
			addAttribute(index, size, type, sizeof(T), (reinterpret_cast<size_t>(&((T*)nullptr->*memberPtr))));
		}

		void addAttribute(unsigned index, int size, AttributeIType type, int stride, size_t offset = 0) {
			glEnableVertexAttribArray(index);
			glVertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(offset));
		}

		unsigned getID(size_t index = 0) const {
			return mVaoId[index];
		}

	private:
		unsigned mVaoId[Count]{};
	};

	using VertexArray = VertexArrays<1>;
}
