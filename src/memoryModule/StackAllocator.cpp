#include "StackAllocator.h"

using namespace Engine::MemoryModule;

StackAllocator::StackAllocator(size_t memSize, const void* mem) : Allocator(memSize, mem) {}

StackAllocator::~StackAllocator() {
	StackAllocator::free(const_cast<void*>(startAddress));
}

void* StackAllocator::allocate(size_t memSize, uint8_t alignment) {
	const size_t currentAddress = reinterpret_cast<size_t>(startAddress) + mOffset;
	const size_t padding = getAdjustment(startAddress, alignment, sizeof(AllocationHeader));

	if (mOffset + padding + memSize > totalSize) {
		return nullptr;
	}
	mOffset += padding;

	const size_t nextAddress = currentAddress + padding;

	mOffset += memSize;

	memoryUsed = mOffset;
	memoryAllocations++;

	return reinterpret_cast<void*>(nextAddress);
}

void StackAllocator::free(void* ptr) {
	const auto curAddress = reinterpret_cast<size_t>(ptr);
	const auto headerAddress = curAddress - sizeof(AllocationHeader);
	const AllocationHeader* allocationHeader {reinterpret_cast<AllocationHeader*>(headerAddress)};

	mOffset = curAddress - allocationHeader->adjustment - reinterpret_cast<size_t>(startAddress);

	memoryUsed = mOffset;
	memoryAllocations--;
}

void StackAllocator::reset() {
	mOffset = 0;
	memoryUsed = 0;
	memoryAllocations = 0;
}