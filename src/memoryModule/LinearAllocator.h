#pragma once
#include "Allocator.h"

namespace GameEngine::MemoryModule {
	class LinearAllocator : public Allocator {
		size_t mOffset = 0;
	public:
		LinearAllocator(size_t memSize, const void* mem);
		~LinearAllocator() override;

		void* allocate(size_t size, uint8_t alignment) override;
		void free(void* mem) override;
		void reset() override;
	};
}