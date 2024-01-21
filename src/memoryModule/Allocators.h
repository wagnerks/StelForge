#pragma once
#include <cstdint>
#include <stddef.h>

namespace Engine::MemoryModule {

	class Allocator {
	public:
		virtual ~Allocator() = default;
		void init(size_t memSize, const void* mem);

		virtual void* allocate(size_t size, uint8_t alignment) = 0;
		virtual void free(void* mem) = 0;
		virtual void reset() = 0;

		size_t getMemorySize() const;
		const void* getStartAddress() const;

	protected:
		size_t mAllocationSize = 0;
		void* mStartAddress = nullptr;
	};

	class LinearAllocator : public Allocator {
	public:
		void* allocate(size_t size, uint8_t alignment) override;
		void free(void* mem) override;
		void reset() override;

	private:
		size_t mOffset = 0;
	};

	class StackAllocator : public Allocator {
	public:
		StackAllocator() = default;

		void* allocate(size_t size, uint8_t alignment) override;
		void free(void* ptr) override;
		void reset() override;

	private:
		size_t mOffset = 0;
	};
}
