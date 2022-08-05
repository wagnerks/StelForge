#include "MemoryChunkAllocator.h"

using namespace GameEngine::MemoryModule;

GlobalMemoryUser::GlobalMemoryUser(MemoryManager* memoryManager) : globalMemoryManager(memoryManager) {}

const void* GlobalMemoryUser::allocate(size_t memSize, size_t user) const {
	return globalMemoryManager->allocate(memSize, user);
}

void GlobalMemoryUser::free(void* pMem) const {
	globalMemoryManager->free(pMem);
}
