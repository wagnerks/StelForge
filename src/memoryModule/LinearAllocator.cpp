#include "LinearAllocator.h"

#include <cassert>

using namespace Engine::MemoryModule;

LinearAllocator::LinearAllocator(size_t memSize, const void* mem) : Allocator(memSize, mem) {}

LinearAllocator::~LinearAllocator() {
	LinearAllocator::free(const_cast<void*>(getStartAddress()));
};

void* LinearAllocator::allocate(size_t memSize, uint8_t alignment) {
	size_t padding = 0;
	const size_t currentAddress = reinterpret_cast<size_t>(startAddress) + mOffset;

	if (alignment != 0 && mOffset % alignment != 0) {
		padding = getAdjustment((void*)currentAddress, alignment);
	}

	if (mOffset + padding + memSize > totalSize) {
		assert(false);
		return nullptr;
	}

	mOffset += padding;
	const size_t nextAddress = currentAddress + padding;
	mOffset += memSize;

	memoryUsed = mOffset;
	memoryAllocations++;

	return reinterpret_cast<void*>(nextAddress);
}

void LinearAllocator::free(void* mem) {
	reset();
}

void LinearAllocator::reset() {
	mOffset = 0;
	memoryUsed = 0;
	memoryAllocations = 0;
}
