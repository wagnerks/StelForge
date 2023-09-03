#pragma once
#include "Allocator.h"

namespace Engine::MemoryModule {
	class StackAllocator : public Allocator {
	public:
		StackAllocator(size_t memSize, const void* mem);
		~StackAllocator() override;

		void* allocate(size_t size, uint8_t alignment) override;
		void free(void* ptr) override;
		void reset() override;
	private:
		size_t mOffset = 0;
		struct AllocationHeader {
			uint8_t adjustment;
		};
	};
}
