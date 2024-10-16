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

	
	template<BufferType Type>
	inline void bindBuffer(unsigned bufferId = 0) {
		glBindBuffer(Type, bufferId);
	}
	template<BufferType Type>
	inline void bindDefaultBuffer() {
		bindBuffer<Type>(0);
	}

	template<BufferType Type>
	struct BindLock {
		BindLock() {}
		~BindLock() { bindDefaultBuffer<Type>(); };
	};

	//https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindBuffer.xhtml
	template<BufferType Type = ARRAY_BUFFER, typename DataType = void, BufferAccessType AccessType = STATIC_DRAW>
	class Buffer {
	public:
		//Buffer(const Buffer& other) = delete;
		//Buffer& operator=(const Buffer& other) = delete;

		Buffer() = default;

		operator bool() const {
			return glIsBuffer(getID());
		}

		~Buffer() {
			release();
		}

		void release() {
			glDeleteBuffers(1, &mId);
		}

		void generate() {
			glGenBuffers(1, &mId);
		}

		BindLock<Type> lock() const {//todo some other interface
			bind();
			return {};
		}

		void bind() const {
			bindBuffer(mId);
		}

		void unbind() const {
			bindDefaultBuffer();
		}

		void setBufferBinding(int index) {
			bindingIdx = index;
			glBindBufferBase(Type, index, mId); //then it can be used in shader using "layout(std140, binding = index) uniform MyBlock"
		}

		void setBufferRange(size_t start, size_t end) {
			glBindBufferRange(Type, bindingIdx, mId, start * sizeof(DataType), (end - start + 1) * sizeof(DataType));
		}

		DataType* mapBuffer() {
			if (mMappedData) {
				return mMappedData;
			}

			mMappedData = static_cast<DataType*>(glMapBufferRange(Type, 0, sizeof(DataType) * mCapacity, GL_MAP_WRITE_BIT));

			return mMappedData;
		}

		void unmapBuffer() {
			mMappedData = nullptr;
			glUnmapBuffer(Type);
		}

		void allocateData(const std::vector<DataType>& data) {
			allocateData(data.size(), data.data());
		}

		void allocateData(size_t count, const DataType* data = nullptr) {
			mCapacity = count;
			glBufferData(Type, sizeof(DataType) * count, static_cast<const void*>(data), AccessType);
			if (mMappedData) {
				unmapBuffer();
				mapBuffer();
			}
		}

		void clear() {
			mSize = 0;
		}

		void shrinkToFit() {
			mCapacity = mSize;

			auto oldData = new DataType[mSize];
			getData(mSize, oldData);
			allocateData(mCapacity);
			setData(mSize, oldData);
			delete[] oldData;
		}

		void reserve(size_t newCapacity) {
			if (newCapacity <= mCapacity) {
				return;
			}

			if (mSize > 0) {
				auto oldData = new DataType[mSize];
				getData(mSize, oldData);
				allocateData(newCapacity);
				setData(mSize, oldData);
				delete[] oldData;
			}
			else {
				allocateData(newCapacity);
			}
		}

		void resize(size_t newSize) {
			if (newSize <= mSize) {
				return;
			}
			reserve(newSize);
			mSize = newSize;
		}

		template<typename Container>
		void setData(Container& container) {
			setData(container.size(), container.data(), 0);
		}

		void setData(size_t count, const DataType* data = nullptr, size_t offset = 0) {
			if (count + offset > mCapacity) {
				reserve(std::max(mCapacity * 2, count + offset));
				mSize = mCapacity;
			}
			if (mMappedData) {
				if (count == 1) {
					mMappedData[offset] = *data;
				}
				else {
					for (auto i = 0u; i < count; i++) {
						mMappedData[offset + i] = data[i];
					}
				}
			}
			else {
				glBufferSubData(Type, offset * sizeof(DataType), count * sizeof(DataType), data);
			}
		}

		void getData(size_t count, DataType* data = nullptr, size_t offset = 0) {
			glGetBufferSubData(Type, offset * sizeof(DataType), count * sizeof(DataType), data);
		}
		template<typename Container>
		void addData(Container& data) {
			addData(data.size(), data.data());
		}

		void addData(size_t count, const DataType* data = nullptr) {
			if (mSize + count > mCapacity) {
				reserve(std::max(mSize + count, mCapacity == 0 ? 1 : mCapacity * 2));
			}
			glBufferSubData(Type, mSize * sizeof(DataType), count * sizeof(DataType), static_cast<const void*>(data));
			mSize += count;
		}

		static void bindBuffer(unsigned bufferId = 0) {
			glBindBuffer(Type, bufferId);
		}

		static void bindDefaultBuffer() {
			bindBuffer(0);
		}

		unsigned getID() const { return mId; }


		size_t size() { return mSize; }
		size_t capacity() { return mCapacity; }

	private:
		unsigned mId = 0;
		size_t mCapacity = 0;
		size_t mSize = 0;
		size_t bindingIdx = 0;

		DataType* mMappedData = nullptr;
	};

	template<typename DataType = void, BufferAccessType AccessType = STATIC_DRAW>
	using ArrayBuffer = Buffer<BufferType::ARRAY_BUFFER, DataType, AccessType>;

	template<typename DataType = void, BufferAccessType AccessType = STATIC_DRAW>
	using ShaderStorageBuffer = Buffer<BufferType::SHADER_STORAGE_BUFFER, DataType, AccessType>; //std430
	
}