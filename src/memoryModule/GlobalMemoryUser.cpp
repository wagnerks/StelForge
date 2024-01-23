#include "GlobalMemoryUser.h"

namespace SFE::MemoryModule {
	GlobalMemoryUser::GlobalMemoryUser(MemoryManager* memoryManager) : mGlobalMemoryManager(memoryManager) {}

	const void* GlobalMemoryUser::allocate(size_t memSize, size_t user) const {
		return mGlobalMemoryManager->allocate(memSize, user);
	}

	void GlobalMemoryUser::free(void* pMem) const {
		mGlobalMemoryManager->free(pMem);
	}
}