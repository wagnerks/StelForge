#pragma once
#include "Allocator.h"

namespace GameEngine::MemoryModule {
	class PoolAllocator : public Allocator {
	public:
		PoolAllocator(size_t memSize, const void* mem, size_t chunkSize, uint8_t chunkAlignment);

		~PoolAllocator() override;

		void* allocate(size_t size, uint8_t alignment) override;
		void free(void* p) override;
		void reset() override;
	private:

		struct FreeHeader {};
		using Node = StackLinkedList<FreeHeader>::Node;
		StackLinkedList<FreeHeader> mFreeList = {};

		const size_t	chunkSize;
		const uint8_t	chunkAlignment;
	};
}
