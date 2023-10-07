#pragma once
#include <queue>

#include "Allocators.h"

namespace ECS::Memory {
	class ECSMemoryStack final {
		ECSMemoryStack(const ECSMemoryStack&) = delete;
		ECSMemoryStack& operator=(ECSMemoryStack&) = delete;

	public:
		ECSMemoryStack(size_t memoryCapacity);
		~ECSMemoryStack();

		void* allocate(size_t memSize);
		void free(void* pMem);

	private:
		const size_t		mMemoryCapacity;
		const void*			mGlobalAddress;

		StackAllocator		mAllocator;
		std::deque<void*>	mPendingMemory;
	};

}
