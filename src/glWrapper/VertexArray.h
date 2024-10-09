#pragma once

#include <cstddef>
#include <glad/glad.h>

namespace SFE::GLW {

	enum class AttributeFType {
		FLOAT = GL_FLOAT,
		DOUBLE = GL_DOUBLE
	};

	enum class AttributeIType {
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
		VertexArrays(const VertexArrays& other) = delete;
		VertexArrays& operator=(const VertexArrays& other) = delete;

		VertexArrays(VertexArrays&& other) noexcept {
			std::memmove(mId, other.mId, Count * sizeof(unsigned));
		}

		VertexArrays& operator=(VertexArrays&& other) noexcept {
			if (this == &other)
				return *this;

			std::memmove(mId, other.mId, Count * sizeof(unsigned));
			return *this;
		}

		operator bool() const {
			return glIsVertexArray(getID(0));
		}
		VertexArrays() {}
		
		~VertexArrays() {
			glDeleteVertexArrays(Count, mId);
		}

		void bind(size_t index = 0) const {
			glBindVertexArray(mId[index]);
		}

		void generate() {
			glGenVertexArrays(Count, mId);
		}

		void release() {
			glDeleteVertexArrays(Count, mId);
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

		//stride is the whole attributes size, for example pos + color strid is 6 floats
		void addAttribute(unsigned index, int size, AttributeFType type, bool normalized, int stride, size_t offset = 0) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, size, static_cast<unsigned>(type), normalized, stride, reinterpret_cast<void*>(offset));
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
			glVertexAttribIPointer(index, size, static_cast<unsigned>(type), stride, reinterpret_cast<void*>(offset));
		}

		unsigned getID(size_t index = 0) const {
			return mId[index];
		}

	private:
		unsigned mId[Count]{};
	};

	using VertexArray = VertexArrays<1>;
}
