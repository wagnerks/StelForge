#pragma once

#include <vector>

#include "glad/glad.h"

namespace SFE::GLW {
	enum BufferType {
		ARRAY_BUFFER = GL_ARRAY_BUFFER,	//Vertex attributes
		ATOMIC_COUNTER_BUFFER = GL_ATOMIC_COUNTER_BUFFER,	//Atomic counter storage
		COPY_READ_BUFFER = GL_COPY_READ_BUFFER,	//Buffer copy source
		COPY_WRITE_BUFFER = GL_COPY_WRITE_BUFFER,	//Buffer copy destination
		DISPATCH_INDIRECT_BUFFER = GL_DISPATCH_INDIRECT_BUFFER,	//Indirect compute dispatch commands
		DRAW_INDIRECT_BUFFER = GL_DRAW_INDIRECT_BUFFER,	//Indirect command arguments
		ELEMENT_ARRAY_BUFFER = GL_ELEMENT_ARRAY_BUFFER,	//Vertex array indices
		PIXEL_PACK_BUFFER = GL_PIXEL_PACK_BUFFER,	//Pixel read target
		PIXEL_UNPACK_BUFFER = GL_PIXEL_UNPACK_BUFFER,	//Texture data source
		QUERY_BUFFER = GL_QUERY_BUFFER,	//Query result buffer
		SHADER_STORAGE_BUFFER = GL_SHADER_STORAGE_BUFFER,	//Read - write storage for shaders
		TEXTURE_BUFFER = GL_TEXTURE_BUFFER,	//Texture data buffer
		TRANSFORM_FEEDBACK_BUFFER = GL_TRANSFORM_FEEDBACK_BUFFER,	//Transform feedback buffer
		UNIFORM_BUFFER = GL_UNIFORM_BUFFER,	//Uniform block storage
	};

	enum BufferAccessType {
		//The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
		STREAM_DRAW = GL_STREAM_DRAW, //The data store contents will be modified once and used at most a few times.
		STATIC_DRAW = GL_STATIC_DRAW, //The data store contents will be modified once and used many times.
		DYNAMIC_DRAW = GL_DYNAMIC_DRAW, //The data store contents will be modified repeatedly and used many times.

		//The data store contents are modified by reading data from the GL, and used to return that data when queried by the application.
		STREAM_READ = GL_STREAM_READ,
		STATIC_READ = GL_STATIC_READ,
		DYNAMIC_READ = GL_DYNAMIC_READ,

		//The data store contents are modified by reading data from the GL, and used as the source for GL drawing and image specification commands.
		STREAM_COPY = GL_STREAM_COPY,
		STATIC_COPY = GL_STATIC_COPY,
		DYNAMIC_COPY = GL_DYNAMIC_COPY,
	};

	struct BindGuard {
		BindGuard(BufferType type) : mType(type) {}
		~BindGuard();

	private:
		BufferType mType;
	};

	//https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindBuffer.xhtml
	template<size_t Count = 1>
	class Buffers {
	public:
		Buffers(const Buffers& other) = delete;

		Buffers(Buffers&& other) noexcept
			: mType(other.mType) {
			std::memmove(mId, other.mId, Count * sizeof(unsigned));
		}

		Buffers& operator=(const Buffers& other) = delete;

		Buffers& operator=(Buffers&& other) noexcept {
			if (this == &other)
				return *this;
			mType = other.mType;
			std::memmove(mId, other.mId, Count * sizeof(unsigned));
			return *this;
		}

		Buffers() = default;

		operator bool() const {
			return glIsBuffer(getID());
		}
		
		Buffers(BufferType type) : mType(type) {
			glGenBuffers(Count, mId);
		}

		~Buffers() {
			release();
		}

		void release() {
			glDeleteBuffers(Count, mId);
		}

		void generate(BufferType type) {
			mType = type;
			glGenBuffers(Count, mId);
		}

		BindGuard bindWithGuard() const {//todo some other interface
			bind();
			return { mType };
		}

		void bind(size_t i = 0) const {
			bindBuffer(mType, mId[i]);
		}

		void unbind() const {
			bindDefaultBuffer(mType);
		}

		void setBufferBinding(int index, size_t bufferIdx = 0) {
			glBindBufferBase(mType, index, mId[bufferIdx]); //then it can be used in shader using "layout(std140, binding = index) uniform MyBlock"
		}

		template<typename T>
		static void allocateData(BufferType type, size_t count, BufferAccessType accessType = STATIC_DRAW, const T* data = nullptr) {
			glBufferData(type, sizeof(T) * count, static_cast<const void*>(data), accessType);
		}

		template<typename T>
		void allocateData(size_t count, BufferAccessType accessType = STATIC_DRAW, const T* data = nullptr) {
			allocateData<T>(mType, count, accessType, data);
		}

		template<typename T>
		void allocateData(const std::vector<T>& data, BufferAccessType accessType = STATIC_DRAW) {
			allocateData<T>(mType, data.size(), accessType, data.data());
		}

		void allocateData(size_t size, size_t count, BufferAccessType accessType = STATIC_DRAW, const void* data = nullptr) const {
			glBufferData(mType, size * count, data, accessType);
		}

		template<typename T>
		void setData(size_t count, const T* data = nullptr, size_t offset = 0) const {
			setData(sizeof(T), count, data, offset);
		}

		void setData(size_t size, size_t count, const void* data = nullptr, size_t offset = 0) const {
			glBufferSubData(mType, offset * size, count * size, data);
		}

		static void bindBuffer(BufferType type, unsigned bufferId = 0) {
			glBindBuffer(type, bufferId);

		}

		static void bindDefaultBuffer(BufferType type) {
			bindBuffer(type, 0);
		}

		unsigned getID(size_t index = 0) const { return mId[index]; }
		BufferType getType() const { return mType; }
	private:
		BufferType mType;
		unsigned mId[Count];
	};

	using Buffer = Buffers<>;
}