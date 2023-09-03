#include "Allocator.h"

using namespace Engine::MemoryModule;

size_t Allocator::getMemorySize() const {
	return totalSize;
}
const void* Allocator::getStartAddress() const {
	return startAddress;
}
size_t Allocator::getUsedMemory() const {
	return memoryUsed;
}
uint64_t Allocator::getAllocationCount() const {
	return memoryAllocations;
}
