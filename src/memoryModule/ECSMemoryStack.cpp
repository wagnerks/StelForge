#include "ECSMemoryStack.h"
#include <cassert>
#include <stdlib.h>

namespace SFE::MemoryModule {
	ECSMemoryStack::ECSMemoryStack(size_t memoryCapacity) : mMemoryCapacity(memoryCapacity), mGlobalAddress(malloc(mMemoryCapacity)) {
		assert(mGlobalAddress);
		if (!mGlobalAddress) {
			return;
		}

		mAllocator.init(mMemoryCapacity, mGlobalAddress);
	}

	ECSMemoryStack::~ECSMemoryStack() {
		while(!mPendingMemory.empty()) { //free all allocated memory
			free(mPendingMemory.back());
		}

		std::free(const_cast<void*>(mGlobalAddress));
	}

	void* ECSMemoryStack::allocate(size_t memSize) {
		void* pMemory = mAllocator.allocate(memSize, 0);
		mPendingMemory.push_back( pMemory);

		return pMemory;
	}

	void ECSMemoryStack::free(void* pMem) {
		if (pMem == mPendingMemory.back()) {
			mAllocator.free(pMem);
			mPendingMemory.pop_back();
		}
		else {
			assert(false);
		}
	}
}

